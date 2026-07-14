#include "SeparationAudioIO.h"
#include "audio/WavIO.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <cstdint>
#include <cstring>

namespace LAStudio {
namespace {
QString ffmpegPath()
{
    const QString configured = qEnvironmentVariable("LASTUDIO_FFMPEG");
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) return configured;
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

WavIO::WavData loadWithQtDecoder(const QString &path)
{
    QAudioDecoder decoder;
    QEventLoop eventLoop;
    QTimer timeout;
    QVector<float> samples;
    QAudioFormat decodedFormat;
    bool failed = false;

    timeout.setSingleShot(true);
    timeout.setInterval(120000);
    QObject::connect(&timeout, &QTimer::timeout, &eventLoop, [&]() {
        failed = true;
        decoder.stop();
        eventLoop.quit();
    });
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, &eventLoop, [&]() {
        const QAudioBuffer buffer = decoder.read();
        if (!buffer.isValid() || buffer.sampleCount() <= 0)
            return;

        if (!decodedFormat.isValid())
            decodedFormat = buffer.format();
        const int count = buffer.sampleCount();
        const int offset = samples.size();
        samples.resize(offset + count);
        float *destination = samples.data() + offset;

        switch (buffer.format().sampleFormat()) {
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
            failed = true;
            decoder.stop();
            eventLoop.quit();
            break;
        }
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, &eventLoop, &QEventLoop::quit);
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),
                     &eventLoop, [&](QAudioDecoder::Error) {
        failed = true;
        eventLoop.quit();
    });

    decoder.setSource(QUrl::fromLocalFile(path));
    decoder.start();
    timeout.start();
    eventLoop.exec();
    timeout.stop();

    WavIO::WavData data;
    if (failed || samples.isEmpty() || !decodedFormat.isValid())
        return data;
    data.samples = std::move(samples);
    data.sampleRate = decodedFormat.sampleRate();
    data.channels = decodedFormat.channelCount();
    return data;
}

WavIO::WavData loadInput(const QString &path, const QString &tempDir)
{
    if (QFileInfo(path).suffix().compare(QStringLiteral("wav"), Qt::CaseInsensitive) == 0) {
        return WavIO::loadAsFloat(path);
    }
    WavIO::WavData decoded = loadWithQtDecoder(path);
    if (!decoded.samples.isEmpty())
        return decoded;

    const QString ffmpegExecutable = ffmpegPath();
    if (ffmpegExecutable.isEmpty())
        return {};
    const QString wavPath = QDir(tempDir).filePath(QStringLiteral("source.wav"));
    QProcess ffmpeg;
    ffmpeg.start(ffmpegExecutable, {QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                                 QStringLiteral("-i"), path, QStringLiteral("-map"), QStringLiteral("0:a:0"),
                                 QStringLiteral("-vn"), QStringLiteral("-ac"), QStringLiteral("2"),
                                 QStringLiteral("-ar"), QStringLiteral("48000"), QStringLiteral("-c:a"),
                                 QStringLiteral("pcm_f32le"), wavPath});
    if (!ffmpeg.waitForStarted(5000) || !ffmpeg.waitForFinished(-1) || ffmpeg.exitCode() != 0) return {};
    return WavIO::loadAsFloat(wavPath);
}
} // namespace

DecodedAudio SeparationAudioIO::decode(const QString &sourcePath, const QString &tempDir)
{
    DecodedAudio result;
    WavIO::WavData wav = loadInput(sourcePath, tempDir);
    if (wav.samples.isEmpty() || wav.channels <= 0 || wav.sampleRate <= 0) {
        return result;
    }
    
    result.sampleRate = wav.sampleRate;
    result.channels.resize(wav.channels);
    const int samplesPerChannel = wav.samples.size() / wav.channels;
    for (int c = 0; c < wav.channels; ++c) {
        result.channels[c].resize(samplesPerChannel);
        for (int i = 0; i < samplesPerChannel; ++i) {
            result.channels[c][i] = wav.samples[i * wav.channels + c];
        }
    }
    return result;
}

bool SeparationAudioIO::saveStem(const QString &path, const QVector<QVector<float>> &channels, int sampleRate)
{
    if (channels.isEmpty() || channels[0].isEmpty()) return false;
    const int n = channels[0].size();
    QVector<float> interleaved;
    interleaved.resize(n * channels.size());
    for (int i = 0; i < n; ++i) {
        for (int c = 0; c < channels.size(); ++c) {
            interleaved[i * channels.size() + c] = channels[c][i];
        }
    }
    
    QString stagingPath = path + QStringLiteral(".staging");
    QFile::remove(stagingPath);
    if (!WavIO::saveFloat(stagingPath, interleaved.constData(), interleaved.size(), sampleRate, channels.size())) {
        return false;
    }
    
    QFile::remove(path);
    if (!QFile::rename(stagingPath, path)) {
        QFile::remove(stagingPath);
        return false;
    }
    return true;
}

} // namespace LAStudio
