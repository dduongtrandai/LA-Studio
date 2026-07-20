#include "AudioFileDecoder.h"

#include "core/Logger.h"

#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

namespace LAStudio {
namespace {

constexpr int kDecodeTimeoutMs = 120000;

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

bool isRiffWave(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray header = file.read(12);
    return header.size() == 12
        && header.first(4) == QByteArrayLiteral("RIFF")
        && header.sliced(8, 4) == QByteArrayLiteral("WAVE");
}

WavIO::WavData decodeWithQt(const QString &path, QString *error)
{
    QAudioDecoder decoder;
    QEventLoop eventLoop;
    QTimer timeout;
    WavIO::WavData result;
    QAudioFormat decodedFormat;
    QString failure;

    timeout.setSingleShot(true);
    timeout.setInterval(kDecodeTimeoutMs);
    QObject::connect(&timeout, &QTimer::timeout, &eventLoop, [&]() {
        failure = QStringLiteral("Audio decode timed out.");
        decoder.stop();
        eventLoop.quit();
    });
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, &eventLoop, [&]() {
        const QAudioBuffer buffer = decoder.read();
        if (!buffer.isValid() || buffer.sampleCount() <= 0)
            return;

        const QAudioFormat format = buffer.format();
        if (!decodedFormat.isValid()) {
            decodedFormat = format;
        } else if (format.sampleRate() != decodedFormat.sampleRate()
                   || format.channelCount() != decodedFormat.channelCount()
                   || format.sampleFormat() != decodedFormat.sampleFormat()) {
            failure = QStringLiteral("Audio format changed while decoding.");
            decoder.stop();
            eventLoop.quit();
            return;
        }

        const int count = buffer.sampleCount();
        const int offset = result.samples.size();
        result.samples.resize(offset + count);
        float *destination = result.samples.data() + offset;

        switch (format.sampleFormat()) {
        case QAudioFormat::UInt8: {
            const auto *source = buffer.constData<quint8>();
            for (int i = 0; i < count; ++i)
                destination[i] = (static_cast<float>(source[i]) - 128.0f) / 128.0f;
            break;
        }
        case QAudioFormat::Int16: {
            const auto *source = buffer.constData<qint16>();
            for (int i = 0; i < count; ++i)
                destination[i] = static_cast<float>(source[i]) / 32768.0f;
            break;
        }
        case QAudioFormat::Int32: {
            const auto *source = buffer.constData<qint32>();
            for (int i = 0; i < count; ++i)
                destination[i] = static_cast<float>(source[i]) / 2147483648.0f;
            break;
        }
        case QAudioFormat::Float:
            std::memcpy(destination, buffer.constData<float>(), count * sizeof(float));
            break;
        default:
            failure = QStringLiteral("Qt Multimedia returned an unsupported PCM sample format.");
            decoder.stop();
            eventLoop.quit();
            break;
        }
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, &eventLoop, &QEventLoop::quit);
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),
                     &eventLoop, [&](QAudioDecoder::Error) {
        failure = decoder.errorString();
        eventLoop.quit();
    });

    decoder.setSource(QUrl::fromLocalFile(path));
    decoder.start();
    timeout.start();
    eventLoop.exec();
    timeout.stop();

    if (!failure.isEmpty() || result.samples.isEmpty() || !decodedFormat.isValid()) {
        setError(error, failure.isEmpty()
                            ? QStringLiteral("Qt Multimedia decoded no audio samples.")
                            : failure);
        return {};
    }

    result.sampleRate = decodedFormat.sampleRate();
    result.channels = decodedFormat.channelCount();
    return result;
}

QString ffmpegPath()
{
    const QString configured = qEnvironmentVariable("LASTUDIO_FFMPEG");
    if (!configured.isEmpty() && QFileInfo(configured).isFile())
        return configured;
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

WavIO::WavData decodeWithFfmpeg(const QString &path, QString *error)
{
    const QString executable = ffmpegPath();
    if (executable.isEmpty()) {
        setError(error, QStringLiteral("FFmpeg is unavailable."));
        return {};
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        setError(error, QStringLiteral("Could not create a temporary audio decode directory."));
        return {};
    }

    const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("decoded.wav"));
    QProcess process;
    process.setProgram(executable);
    process.setArguments({QStringLiteral("-hide_banner"),
                          QStringLiteral("-nostdin"),
                          QStringLiteral("-y"),
                          QStringLiteral("-i"), path,
                          QStringLiteral("-map"), QStringLiteral("0:a:0"),
                          QStringLiteral("-vn"),
                          QStringLiteral("-c:a"), QStringLiteral("pcm_f32le"),
                          outputPath});
    process.start();
    if (!process.waitForStarted(5000)) {
        setError(error, QStringLiteral("Could not start FFmpeg."));
        return {};
    }
    if (!process.waitForFinished(kDecodeTimeoutMs)) {
        process.kill();
        process.waitForFinished();
        setError(error, QStringLiteral("FFmpeg audio decode timed out."));
        return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (detail.length() > 500)
            detail = detail.right(500);
        setError(error, detail.isEmpty()
                            ? QStringLiteral("FFmpeg could not decode the audio stream.")
                            : QStringLiteral("FFmpeg could not decode the audio stream: %1").arg(detail));
        return {};
    }

    WavIO::WavData result = WavIO::loadAsFloat(outputPath);
    if (result.samples.isEmpty())
        setError(error, QStringLiteral("FFmpeg produced an invalid WAV file."));
    return result;
}

} // namespace

WavIO::WavData AudioFileDecoder::decode(const QString &path, QString *error)
{
    if (error)
        error->clear();

    const QString localPath = QDir::toNativeSeparators(path);
    if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
        setError(error, QStringLiteral("Audio file was not found: %1").arg(localPath));
        return {};
    }

    if (isRiffWave(localPath)) {
        WavIO::WavData wav = WavIO::loadAsFloat(localPath);
        if (!wav.samples.isEmpty())
            return wav;
    }

    QString qtError;
    WavIO::WavData decoded = decodeWithQt(localPath, &qtError);
    if (!decoded.samples.isEmpty())
        return decoded;

    QString ffmpegError;
    decoded = decodeWithFfmpeg(localPath, &ffmpegError);
    if (!decoded.samples.isEmpty())
        return decoded;

    const QString message = QStringLiteral("Unsupported or invalid audio file: %1. Qt: %2 FFmpeg: %3")
                                .arg(localPath, qtError, ffmpegError);
    Logger::error(QStringLiteral("AudioFileDecoder"), message);
    setError(error, message);
    return {};
}

WavIO::WavData AudioFileDecoder::decodeMono(const QString &path,
                                            int targetSampleRate,
                                            QString *error)
{
    WavIO::WavData audio = decode(path, error);
    if (audio.samples.isEmpty())
        return audio;
    if (audio.channels <= 0 || audio.sampleRate <= 0 || targetSampleRate <= 0) {
        setError(error, QStringLiteral("Decoded audio has an invalid channel count or sample rate."));
        return {};
    }

    if (audio.channels > 1) {
        const int frameCount = audio.samples.size() / audio.channels;
        QVector<float> mono(frameCount);
        for (int frame = 0; frame < frameCount; ++frame) {
            float sum = 0.0f;
            for (int channel = 0; channel < audio.channels; ++channel)
                sum += audio.samples[frame * audio.channels + channel];
            mono[frame] = sum / audio.channels;
        }
        audio.samples = std::move(mono);
        audio.channels = 1;
    }

    if (audio.sampleRate != targetSampleRate) {
        const double ratio = static_cast<double>(targetSampleRate) / audio.sampleRate;
        const int outputSize = std::max(1, static_cast<int>(std::llround(audio.samples.size() * ratio)));
        const int sourceSize = static_cast<int>(audio.samples.size());
        QVector<float> resampled(outputSize);
        for (int i = 0; i < outputSize; ++i) {
            const double sourcePosition = i / ratio;
            const int first = std::min(static_cast<int>(sourcePosition), sourceSize - 1);
            const int second = std::min(first + 1, sourceSize - 1);
            const double fraction = sourcePosition - first;
            resampled[i] = static_cast<float>(audio.samples[first] * (1.0 - fraction)
                                              + audio.samples[second] * fraction);
        }
        audio.samples = std::move(resampled);
        audio.sampleRate = targetSampleRate;
    }

    return audio;
}

} // namespace LAStudio
