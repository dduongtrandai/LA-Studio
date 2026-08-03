#include "AudioPreviewService.h"

#include "core/PathUtils.h"
#include "tts/TtsEngine.h"
#include "audio/AudioFileDecoder.h"
#include "audio/AudioPlayer.h"
#include "audio/WaveformProvider.h"
#include "audio/WavIO.h"
#include "core/Logger.h"

#include <QThreadPool>
#include <QMetaObject>
#include <QDir>
#include <QPointer>
#include <QCoreApplication>
#include <algorithm>

namespace LAStudio {

AudioPreviewService::AudioPreviewService(TtsEngine* tts, AudioPlayer* player, WaveformProvider* waveformProvider, QObject *parent)
    : QObject(parent)
    , m_tts(tts)
    , m_player(player)
    , m_waveformProvider(waveformProvider)
{
}

void AudioPreviewService::playLastTts()
{
    playLastTtsAtPosition(0);
}

void AudioPreviewService::playLastTtsAtPosition(qint64 positionMs)
{
    if (!m_tts || !m_player || !m_waveformProvider) {
        return;
    }

    if (m_tts->lastPcm().isEmpty()) {
        emit errorOccurred(QStringLiteral("No TTS audio to play"));
        return;
    }
    m_player->playPcm(m_tts->lastPcm(), m_tts->sampleRate());
    if (positionMs > 0)
        m_player->seek(positionMs);
    m_waveformProvider->setSamples(m_tts->lastSamples());
}

void AudioPreviewService::requestWavSamples(const QString &path)
{
    const QString sourcePath = path;
    QString cleanPath = PathUtils::urlToLocalPath(sourcePath);
    cleanPath = QDir::toNativeSeparators(cleanPath);

    const quint64 requestId = ++m_wavSamplesRequestId;

    if (sourcePath.isEmpty()) {
        if (!m_wavSamples.isEmpty() || !m_wavSamplesSourcePath.isEmpty()) {
            m_wavSamples.clear();
            m_wavSamplesSourcePath.clear();
            emit wavSamplesChanged();
        }
        if (m_wavSamplesLoading) {
            m_wavSamplesLoading = false;
            emit wavSamplesLoadingChanged();
        }
        return;
    }

    if (sourcePath == m_wavSamplesSourcePath && !m_wavSamples.isEmpty()) {
        if (m_wavSamplesLoading) {
            m_wavSamplesLoading = false;
            emit wavSamplesLoadingChanged();
        }
        return;
    }

    m_wavSamplesLoading = true;
    emit wavSamplesLoadingChanged();

    QPointer<AudioPreviewService> weakThis(this);
    QThreadPool::globalInstance()->start([weakThis, cleanPath, sourcePath, requestId]() {
        QString decodeError;
        const WavIO::WavData data = AudioFileDecoder::decode(cleanPath, &decodeError);
        QVariantList list;
        if (!data.samples.isEmpty() && data.channels > 0) {
            const int frameCount = data.samples.size() / data.channels;
            const int step = std::max(1, frameCount / 1000);
            list.reserve(frameCount / step + 1);
            for (int frame = 0; frame < frameCount; frame += step) {
                float value = 0.0f;
                for (int channel = 0; channel < data.channels; ++channel)
                    value += data.samples[frame * data.channels + channel];
                list.append(value / data.channels);
            }
        }

        QCoreApplication* app = QCoreApplication::instance();
        if (app) {
            QMetaObject::invokeMethod(app, [weakThis, requestId, sourcePath, list, decodeError]() {
                if (!weakThis)
                    return;

                if (requestId != weakThis->m_wavSamplesRequestId)
                    return;

                if (list.isEmpty() && !decodeError.isEmpty()) {
                    Logger::warning(QStringLiteral("AudioPreviewService"),
                                    QStringLiteral("Audio preview decode failed: %1").arg(decodeError));
                }

                weakThis->m_wavSamples = list;
                weakThis->m_wavSamplesSourcePath = sourcePath;
                emit weakThis->wavSamplesChanged();

                if (weakThis->m_wavSamplesLoading) {
                    weakThis->m_wavSamplesLoading = false;
                    emit weakThis->wavSamplesLoadingChanged();
                }
            });
        }
    });
}

void AudioPreviewService::saveWav(const QString &path)
{
    if (!m_tts) {
        return;
    }

    if (m_tts->lastSamples().isEmpty()) {
        emit errorOccurred(QStringLiteral("No audio to save"));
        return;
    }

    QString savePath = PathUtils::urlToLocalPath(path);
    QVector<float> samples = m_tts->lastSamples();
    int sampleRate = m_tts->sampleRate();

    QPointer<AudioPreviewService> weakThis(this);
    QThreadPool::globalInstance()->start([weakThis, savePath, samples, sampleRate]() {
        bool ok = WavIO::saveFloat(savePath, samples.constData(),
                                    samples.size(), sampleRate);
        if (!ok) {
            QCoreApplication* app = QCoreApplication::instance();
            if (app) {
                QMetaObject::invokeMethod(app, [weakThis]() {
                    if (weakThis) {
                        emit weakThis->errorOccurred(QStringLiteral("Failed to save WAV file"));
                    }
                });
            }
        }
    });
}

} // namespace LAStudio
