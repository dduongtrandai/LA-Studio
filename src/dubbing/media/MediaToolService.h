#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

namespace LAStudio {

// Small process boundary for media operations. Keeping FFmpeg behind this
// class lets the controller stay testable and avoids shell-string execution.
class MediaToolService final : public QObject
{
    Q_OBJECT
public:
    explicit MediaToolService(QObject *parent = nullptr);

    QString executablePath() const;
    bool available() const;
    void muxVideoWithAudio(const QString &videoPath,
                           const QString &audioPath,
                           const QString &outputPath);
    void cancel();

signals:
    void finished(bool success, const QString &outputPath, const QString &error);
    void progress(int percent);

private slots:
    void onReadyReadStandardError();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess m_process;
    QString m_outputPath;
    QByteArray m_stderr;
};

} // namespace LAStudio
