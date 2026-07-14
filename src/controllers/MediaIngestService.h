#pragma once

#include <QObject>
#include <QProcess>
#include <QVariantMap>

namespace LAStudio {

// Imports one media file into a content-addressed dubbing workspace. The
// service owns probing and audio normalization so controllers never need to
// parse FFmpeg output or pass an unnormalized source to STT.
class MediaIngestService final : public QObject
{
    Q_OBJECT
public:
    explicit MediaIngestService(QObject *parent = nullptr);

    bool available() const;
    void ingest(const QString &path);
    void cancel();

signals:
    void progress(int percent);
    void finished(bool success, const QVariantMap &manifest, const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onReadyReadStandardError();

private:
    enum class Stage { None, Probe, Master, Analysis };
    void fail(const QString &error);
    void startProbe();
    void startMaster();
    void startAnalysis();
    QString ffmpegPath() const;
    QString ffprobePath() const;

    QProcess m_process;
    QString m_inputPath;
    QString m_hash;
    QString m_workspace;
    QString m_masterPath;
    QString m_analysisPath;
    QByteArray m_stderr;
    QByteArray m_probeOutput;
    QVariantMap m_manifest;
    Stage m_stage = Stage::None;
};

} // namespace LAStudio
