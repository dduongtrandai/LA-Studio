#include "LongFormAudioSource.h"

#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtEndian>
#include <cstring>

namespace LAStudio {

namespace {

bool isRawFloatPath(const QString &path)
{
    return path.endsWith(QStringLiteral(".f32le"), Qt::CaseInsensitive);
}

void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}

bool runProcess(QProcess &process, int timeoutMs, QString *error)
{
    if (!process.waitForStarted(10000)) {
        setError(error, QStringLiteral("Could not start %1.").arg(process.program()));
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        setError(error, QStringLiteral("%1 timed out.").arg(process.program()));
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (detail.size() > 600) detail = detail.right(600);
        setError(error, detail.isEmpty()
                    ? QStringLiteral("%1 failed.").arg(process.program())
                    : detail);
        return false;
    }
    return true;
}

} // namespace

QString LongFormAudioSource::ffmpegExecutable()
{
    const QString configured = qEnvironmentVariable("LASTUDIO_FFMPEG");
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) return configured;
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

QString LongFormAudioSource::ffprobeExecutable()
{
    const QString configured = qEnvironmentVariable("LASTUDIO_FFPROBE");
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) return configured;
    return QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
}

bool LongFormAudioSource::probeDurationMs(const QString &path, qint64 &durationMs,
                                          QString *error)
{
    durationMs = 0;
    if (error) error->clear();
    if (!QFileInfo(path).isFile()) {
        setError(error, QStringLiteral("Audio file was not found: %1").arg(path));
        return false;
    }
    if (isRawFloatPath(path)) {
        const qint64 bytes = QFileInfo(path).size();
        durationMs = qRound64(double(bytes) / (sizeof(float) * 16000.0) * 1000.0);
        if (durationMs <= 0) {
            setError(error, QStringLiteral("The normalized audio cache is empty."));
            return false;
        }
        if (error) error->clear();
        return true;
    }

    const QString ffprobe = ffprobeExecutable();
    if (!ffprobe.isEmpty()) {
        QProcess process;
        process.setProgram(ffprobe);
        process.setArguments({QStringLiteral("-v"), QStringLiteral("error"),
                               QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
                               QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
                               path});
        if (runProcess(process, 30000, error)) {
            bool ok = false;
            const double seconds = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toDouble(&ok);
            if (ok && seconds > 0.0) {
                durationMs = qMax<qint64>(1, qRound64(seconds * 1000.0));
                if (error) error->clear();
                return true;
            }
        }
    }

    const QString ffmpeg = ffmpegExecutable();
    if (ffmpeg.isEmpty()) {
        setError(error, QStringLiteral("FFmpeg/FFprobe is unavailable for long-form audio."));
        return false;
    }
    QProcess process;
    process.setProgram(ffmpeg);
    process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-i"), path});
    if (!process.waitForStarted(10000)) {
        setError(error, QStringLiteral("Could not start FFmpeg."));
        return false;
    }
    process.waitForFinished(30000);
    const QString stderrText = QString::fromUtf8(process.readAllStandardError());
    const QRegularExpression re(QStringLiteral("Duration:\\s+(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)"));
    const auto match = re.match(stderrText);
    if (!match.hasMatch()) {
        setError(error, QStringLiteral("Could not determine audio duration."));
        return false;
    }
    const double seconds = match.captured(1).toDouble() * 3600.0
                         + match.captured(2).toDouble() * 60.0
                         + match.captured(3).toDouble();
    durationMs = qMax<qint64>(1, qRound64(seconds * 1000.0));
    if (error) error->clear();
    return true;
}

bool LongFormAudioSource::prepareMono16k(const QString &sourcePath,
                                         const QString &cacheRoot,
                                         QString &normalizedPath,
                                         qint64 &durationMs,
                                         QString *error)
{
    normalizedPath.clear();
    durationMs = 0;
    if (error) error->clear();
    if (!QFileInfo(sourcePath).isFile()) {
        setError(error, QStringLiteral("Audio file was not found: %1").arg(sourcePath));
        return false;
    }
    if (isRawFloatPath(sourcePath)) {
        normalizedPath = sourcePath;
        return probeDurationMs(sourcePath, durationMs, error);
    }

    const QFileInfo info(sourcePath);
    QByteArray identity = QDir::fromNativeSeparators(info.absoluteFilePath()).toUtf8();
    identity += '\n' + QByteArray::number(info.size()) + '\n' + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    const QString key = QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
    QDir root(cacheRoot);
    if (!root.mkpath(QStringLiteral("."))) {
        setError(error, QStringLiteral("Could not create the long-form audio cache directory."));
        return false;
    }
    normalizedPath = root.filePath(key + QStringLiteral(".f32le"));
    const QFileInfo cached(normalizedPath);
    if (!cached.isFile() || cached.size() < qint64(sizeof(float) * 16000)) {
        const QString ffmpeg = ffmpegExecutable();
        if (ffmpeg.isEmpty()) {
            setError(error, QStringLiteral("FFmpeg is unavailable for normalized audio caching."));
            normalizedPath.clear();
            return false;
        }
        const QString temporaryPath = normalizedPath + QStringLiteral(".partial");
        QFile::remove(temporaryPath);
        QProcess process;
        process.setProgram(ffmpeg);
        process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                              QStringLiteral("-i"), sourcePath, QStringLiteral("-vn"),
                              QStringLiteral("-ac"), QStringLiteral("1"), QStringLiteral("-ar"), QStringLiteral("16000"),
                              QStringLiteral("-f"), QStringLiteral("f32le"), QStringLiteral("-" )});
        process.setStandardOutputFile(temporaryPath);
        if (!runProcess(process, 6 * 60 * 60 * 1000, error)) {
            QFile::remove(temporaryPath);
            normalizedPath.clear();
            return false;
        }
        if (!QFile::rename(temporaryPath, normalizedPath)) {
            QFile::remove(temporaryPath);
            setError(error, QStringLiteral("Could not commit the normalized audio cache."));
            normalizedPath.clear();
            return false;
        }
    }
    return probeDurationMs(normalizedPath, durationMs, error);
}

QVector<float> LongFormAudioSource::readMono16kRange(const QString &path,
                                                     qint64 startMs,
                                                     qint64 durationMs,
                                                     QString *error)
{
    if (error) error->clear();
    QVector<float> result;
    if (isRawFloatPath(path)) {
        QFile file(path);
        const qint64 startSample = qMax<qint64>(0, startMs) * 16000 / 1000;
        const qint64 requestedSamples = qMax<qint64>(1, durationMs) * 16000 / 1000;
        if (!file.open(QIODevice::ReadOnly) || !file.seek(startSample * qint64(sizeof(float)))) {
            setError(error, QStringLiteral("Could not seek normalized audio cache."));
            return result;
        }
        const QByteArray bytes = file.read(requestedSamples * qint64(sizeof(float)));
        const qsizetype count = bytes.size() / qsizetype(sizeof(float));
        if (count <= 0) {
            setError(error, QStringLiteral("Normalized audio cache returned no samples."));
            return result;
        }
        result.resize(int(count));
        std::memcpy(result.data(), bytes.constData(), size_t(count * sizeof(float)));
        return result;
    }
    const QString ffmpeg = ffmpegExecutable();
    if (ffmpeg.isEmpty()) {
        setError(error, QStringLiteral("FFmpeg is unavailable for long-form audio."));
        return result;
    }
    const double start = qMax<qint64>(0, startMs) / 1000.0;
    const double duration = qMax<qint64>(1, durationMs) / 1000.0;
    QProcess process;
    process.setProgram(ffmpeg);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
                          QStringLiteral("-ss"), QString::number(start, 'f', 3),
                          QStringLiteral("-t"), QString::number(duration, 'f', 3),
                          QStringLiteral("-i"), path, QStringLiteral("-vn"),
                          QStringLiteral("-ac"), QStringLiteral("1"),
                          QStringLiteral("-ar"), QStringLiteral("16000"),
                          QStringLiteral("-f"), QStringLiteral("f32le"), QStringLiteral("-" )});
    if (!runProcess(process, qMax(120000, int(durationMs * 8)), error)) return result;

    const QByteArray bytes = process.readAllStandardOutput();
    const qsizetype sampleBytes = qsizetype(sizeof(float));
    const qsizetype count = bytes.size() / sampleBytes;
    if (count <= 0) {
        setError(error, QStringLiteral("FFmpeg returned no audio samples for the requested range."));
        return result;
    }
    result.resize(int(count));
    std::memcpy(result.data(), bytes.constData(), size_t(count * sampleBytes));
    return result;
}

} // namespace LAStudio
