#include "SttAudioDecoder.h"
#include "audio/WavIO.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include <QUrl>
#include <cmath>
#include <algorithm>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace LAStudio {

SttAudioDecoder::SttAudioDecoder(QObject *parent)
    : QObject(parent)
{
    connect(&m_decoder, &QAudioDecoder::bufferReady, this, &SttAudioDecoder::handleBufferReady);
    connect(&m_decoder, &QAudioDecoder::finished, this, &SttAudioDecoder::handleFinished);
    connect(&m_decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this, &SttAudioDecoder::handleError);
}

void SttAudioDecoder::startDecode(const QString &filePath)
{
    m_filePath = filePath;
    m_decodedSamples.clear();

    QString localPath = PathUtils::urlToLocalPath(filePath);

    // Fast-path for WAV files using WavIO on a worker thread
    if (localPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        Logger::info("SttAudioDecoder", "Decoding WAV file using fast-path WavIO in worker thread: " + localPath);
        
        auto* watcher = new QFutureWatcher<WavIO::WavData>(this);
        connect(watcher, &QFutureWatcher<WavIO::WavData>::finished, this, [this, watcher, localPath]() {
            watcher->deleteLater();
            WavIO::WavData data = watcher->result();
            if (!data.samples.isEmpty()) {
                Logger::info("SttAudioDecoder", "Finished fast-path WAV decoding: " + localPath);
                emit finished(data.samples);
            } else {
                Logger::warning("SttAudioDecoder", "Fast-path WAV decoding failed, falling back to QAudioDecoder: " + localPath);
                m_decoder.setSource(QUrl::fromLocalFile(localPath));
                m_decoder.start();
            }
        });

        QFuture<WavIO::WavData> future = QtConcurrent::run([localPath]() {
            return WavIO::loadAsFloatMono16k(localPath);
        });
        watcher->setFuture(future);
        return;
    }

    Logger::info("SttAudioDecoder", "Decoding file using QAudioDecoder: " + localPath);
    m_decoder.setSource(QUrl::fromLocalFile(localPath));
    m_decoder.start();
}

void SttAudioDecoder::handleBufferReady()
{
    QAudioBuffer buffer = m_decoder.read();
    if (!buffer.isValid()) return;

    QAudioFormat format = buffer.format();
    int sampleCount = buffer.sampleCount();

    if (sampleCount <= 0) return;

    int currentSize = m_decodedSamples.size();
    m_decodedSamples.resize(currentSize + sampleCount);
    float* dest = m_decodedSamples.data() + currentSize;

    if (format.sampleFormat() == QAudioFormat::Int16) {
        const int16_t* src = buffer.constData<int16_t>();
        for (int i = 0; i < sampleCount; ++i) {
            dest[i] = static_cast<float>(src[i]) / 32768.0f;
        }
    } else if (format.sampleFormat() == QAudioFormat::Float) {
        const float* src = buffer.constData<float>();
        memcpy(dest, src, sampleCount * sizeof(float));
    } else if (format.sampleFormat() == QAudioFormat::Int32) {
        const int32_t* src = buffer.constData<int32_t>();
        for (int i = 0; i < sampleCount; ++i) {
            dest[i] = static_cast<float>(src[i]) / 2147483648.0f;
        }
    } else if (format.sampleFormat() == QAudioFormat::UInt8) {
        const uint8_t* src = buffer.constData<uint8_t>();
        for (int i = 0; i < sampleCount; ++i) {
            dest[i] = static_cast<float>(src[i] - 128) / 128.0f;
        }
    } else {
        m_decoder.stop();
        emit errorOccurred(QStringLiteral("Unsupported sample format from QAudioDecoder"));
        return;
    }
}

void SttAudioDecoder::handleFinished()
{
    QAudioFormat format = m_decoder.audioFormat();
    int channels = format.channelCount();
    int sampleRate = format.sampleRate();

    if (channels <= 0 || sampleRate <= 0 || m_decodedSamples.isEmpty()) {
        emit errorOccurred(QStringLiteral("No audio data was decoded."));
        return;
    }

    // Step 1: Downmix stereo/multichannel to mono
    QVector<float> monoSamples;
    if (channels > 1) {
        int frameCount = m_decodedSamples.size() / channels;
        monoSamples.resize(frameCount);
        for (int i = 0; i < frameCount; ++i) {
            float sum = 0.0f;
            for (int c = 0; c < channels; ++c) {
                sum += m_decodedSamples[i * channels + c];
            }
            monoSamples[i] = sum / static_cast<float>(channels);
        }
    } else {
        monoSamples = std::move(m_decodedSamples);
    }
    m_decodedSamples.clear();

    // Step 2: Resample to 16000 Hz
    QVector<float> resampled;
    if (sampleRate == 16000) {
        resampled = std::move(monoSamples);
    } else {
        double ratio = 16000.0 / sampleRate;
        int newLen = static_cast<int>(monoSamples.size() * ratio);
        resampled.resize(newLen);
        for (int i = 0; i < newLen; ++i) {
            double srcIdx = i / ratio;
            int idx0 = static_cast<int>(srcIdx);
            int idx1 = std::min(idx0 + 1, static_cast<int>(monoSamples.size() - 1));
            double frac = srcIdx - idx0;
            resampled[i] = static_cast<float>(monoSamples[idx0] * (1.0 - frac) +
                                               monoSamples[idx1] * frac);
        }
    }

    emit finished(resampled);
}

void SttAudioDecoder::handleError(QAudioDecoder::Error error)
{
    Q_UNUSED(error);
    emit errorOccurred(QStringLiteral("Decoding error: ") + m_decoder.errorString());
}

} // namespace LAStudio
