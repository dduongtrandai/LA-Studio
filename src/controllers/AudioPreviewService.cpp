#include "AudioPreviewService.h"

#include "core/PathUtils.h"
#include "tts/TtsEngine.h"
#include "audio/AudioPlayer.h"
#include "audio/WaveformProvider.h"
#include "audio/WavIO.h"
#include "core/Logger.h"

#include <QThreadPool>
#include <QMetaObject>
#include <QDir>
#include <QPointer>
#include <QCoreApplication>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QUrl>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace LAStudio {

AudioPreviewService::AudioPreviewService(TtsEngine* tts, AudioPlayer* player, WaveformProvider* waveformProvider, QObject *parent)
    : QObject(parent)
    , m_tts(tts)
    , m_player(player)
    , m_waveformProvider(waveformProvider)
{
    connect(&m_decoder, &QAudioDecoder::bufferReady,
            this, &AudioPreviewService::handleDecoderBufferReady);
    connect(&m_decoder, &QAudioDecoder::finished,
            this, &AudioPreviewService::handleDecoderFinished);
    connect(&m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),
            this, &AudioPreviewService::handleDecoderError);
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
        m_decoder.stop();
        m_decodedSamples.clear();
        m_decoderSourcePath.clear();
        m_decoderRequestId = requestId;
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

    m_decoder.stop();
    m_decodedSamples.clear();

    // WavIO is faster and more reliable for WAV. Use Qt Multimedia for
    // compressed audio such as MP3/M4A, which WavIO cannot decode.
    if (!cleanPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        m_decoderSourcePath = sourcePath;
        m_decoderRequestId = requestId;
        m_decoder.setSource(QUrl::fromLocalFile(cleanPath));
        m_decoder.start();
        return;
    }

    QPointer<AudioPreviewService> weakThis(this);
    QThreadPool::globalInstance()->start([weakThis, cleanPath, sourcePath, requestId]() {
        WavIO::WavData data = WavIO::loadAsFloat(cleanPath);
        QVariantList list;
        if (!data.samples.isEmpty()) {
            int step = std::max<int>(1, data.samples.size() / 1000);
            list.reserve(data.samples.size() / step + 1);
            for (int i = 0; i < data.samples.size(); i += step) {
                list.append(data.samples[i]);
            }
        }

        QCoreApplication* app = QCoreApplication::instance();
        if (app) {
            QMetaObject::invokeMethod(app, [weakThis, requestId, sourcePath, list]() {
                if (!weakThis)
                    return;

                if (requestId != weakThis->m_wavSamplesRequestId)
                    return;

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

void AudioPreviewService::handleDecoderBufferReady()
{
    const QAudioBuffer buffer = m_decoder.read();
    if (!buffer.isValid() || buffer.sampleCount() <= 0)
        return;

    const QAudioFormat format = buffer.format();
    const int count = buffer.sampleCount();
    const int offset = m_decodedSamples.size();
    m_decodedSamples.resize(offset + count);
    float *destination = m_decodedSamples.data() + offset;

    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8: {
        const auto *source = buffer.constData<uint8_t>();
        for (int i = 0; i < count; ++i)
            destination[i] = (static_cast<float>(source[i]) - 128.0f) / 128.0f;
        break;
    }
    case QAudioFormat::Int16: {
        const auto *source = buffer.constData<int16_t>();
        for (int i = 0; i < count; ++i)
            destination[i] = static_cast<float>(source[i]) / 32768.0f;
        break;
    }
    case QAudioFormat::Int32: {
        const auto *source = buffer.constData<int32_t>();
        for (int i = 0; i < count; ++i)
            destination[i] = static_cast<float>(source[i]) / 2147483648.0f;
        break;
    }
    case QAudioFormat::Float:
        std::memcpy(destination, buffer.constData<float>(), count * sizeof(float));
        break;
    default:
        m_decoder.stop();
        m_decodedSamples.clear();
        break;
    }
}

void AudioPreviewService::handleDecoderFinished()
{
    publishDecodedSamples(m_decoderSourcePath, m_decoderRequestId);
}

void AudioPreviewService::handleDecoderError(QAudioDecoder::Error error)
{
    Q_UNUSED(error);
    Logger::warning("AudioPreviewService", "Audio preview decode failed: " + m_decoder.errorString());
    m_decodedSamples.clear();
    publishDecodedSamples(m_decoderSourcePath, m_decoderRequestId);
}

void AudioPreviewService::publishDecodedSamples(const QString &sourcePath, quint64 requestId)
{
    if (requestId != m_wavSamplesRequestId)
        return;

    const QAudioFormat format = m_decoder.audioFormat();
    const int channels = std::max(1, format.channelCount());
    const int frameCount = m_decodedSamples.size() / channels;
    QVariantList list;
    if (frameCount > 0) {
        const int step = std::max(1, frameCount / 1000);
        list.reserve(frameCount / step + 1);
        for (int frame = 0; frame < frameCount; frame += step) {
            float value = 0.0f;
            for (int channel = 0; channel < channels; ++channel)
                value += m_decodedSamples[frame * channels + channel];
            list.append(value / channels);
        }
    }

    m_wavSamples = list;
    m_wavSamplesSourcePath = sourcePath;
    emit wavSamplesChanged();
    m_wavSamplesLoading = false;
    emit wavSamplesLoadingChanged();
    m_decodedSamples.clear();
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
