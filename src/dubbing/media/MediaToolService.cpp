#include "dubbing/media/MediaToolService.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace LAStudio {

MediaToolService::MediaToolService(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &MediaToolService::onReadyReadStandardError);
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &MediaToolService::onFinished);
}

QString MediaToolService::executablePath() const
{
    const QString configured = qEnvironmentVariable("LASTUDIO_FFMPEG");
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) return configured;
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

bool MediaToolService::available() const
{
    return !executablePath().isEmpty();
}

void MediaToolService::muxVideoWithAudio(const QString &videoPath,
                                         const QString &audioPath,
                                         const QString &subtitlePath,
                                         const QString &outputPath)
{
    if (m_process.state() != QProcess::NotRunning) {
        emit finished(false, outputPath, QStringLiteral("Another media operation is already running."));
        return;
    }
    const QString executable = executablePath();
    if (executable.isEmpty()) {
        emit finished(false, outputPath,
                      QStringLiteral("FFmpeg was not found. Install the managed media runtime or set LASTUDIO_FFMPEG."));
        return;
    }
    if (!QFileInfo(videoPath).isFile() || !QFileInfo(audioPath).isFile()) {
        emit finished(false, outputPath, QStringLiteral("Video or dubbing audio file does not exist."));
        return;
    }

    m_outputPath = outputPath;
    m_stderr.clear();
    m_process.setProgram(executable);
    m_process.setWorkingDirectory(QFileInfo(executable).absolutePath());
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    QStringList arguments{
        QStringLiteral("-hide_banner"), QStringLiteral("-y"),
        QStringLiteral("-i"), videoPath,
        QStringLiteral("-i"), audioPath,
    };
    const bool hasSubtitles = !subtitlePath.isEmpty() && QFileInfo(subtitlePath).isFile();
    if (hasSubtitles)
        arguments.append({QStringLiteral("-i"), subtitlePath});
    arguments.append({
        QStringLiteral("-map"), QStringLiteral("0:v:0?"),
        QStringLiteral("-map"), QStringLiteral("1:a:0"),
        QStringLiteral("-c:v"), QStringLiteral("copy"),
        QStringLiteral("-c:a"), QStringLiteral("aac"),
        QStringLiteral("-b:a"), QStringLiteral("192k")
    });
    if (hasSubtitles) {
        const QString suffix = QFileInfo(outputPath).suffix().toLower();
        const QString codec = suffix == QStringLiteral("webm")
            ? QStringLiteral("webvtt")
            : (suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mov")
               || suffix == QStringLiteral("m4v"))
                ? QStringLiteral("mov_text") : QStringLiteral("srt");
        arguments.append({QStringLiteral("-map"), QStringLiteral("2:0"),
                          QStringLiteral("-c:s"), codec,
                          QStringLiteral("-metadata:s:s:0"), QStringLiteral("title=Dubbed subtitles"),
                          QStringLiteral("-disposition:s:0"), QStringLiteral("default")});
    }
    arguments.append(outputPath);
    m_process.setArguments(arguments);
    connect(&m_process, &QProcess::started, this, [this]() { emit progress(5); });
    m_process.start();
    if (!m_process.waitForStarted(5000)) {
        emit finished(false, outputPath, QStringLiteral("FFmpeg could not be started: %1").arg(m_process.errorString()));
    }
}

void MediaToolService::cancel()
{
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
}

void MediaToolService::onReadyReadStandardError()
{
    m_stderr += m_process.readAllStandardError();
    if (m_stderr.size() > 1024 * 1024) m_stderr = m_stderr.right(1024 * 1024);
    // FFmpeg's machine-readable progress is intentionally not required for
    // correctness; expose a monotonic indeterminate-progress marker instead.
    emit progress(50);
}

void MediaToolService::onFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool ok = status == QProcess::NormalExit && exitCode == 0 && QFileInfo(m_outputPath).isFile();
    const QString error = ok ? QString() : QStringLiteral("FFmpeg export failed: %1")
        .arg(QString::fromLocal8Bit(m_stderr).trimmed());
    emit progress(ok ? 100 : 0);
    emit finished(ok, m_outputPath, error);
    m_outputPath.clear();
    m_stderr.clear();
}

} // namespace LAStudio
