#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QAtomicInteger>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

namespace LAStudio {

class MediaToolService;

class DubbingExportJob final : public QObject
{
    Q_OBJECT
public:
    explicit DubbingExportJob(QObject *parent = nullptr);
    ~DubbingExportJob() override;

    bool running() const { return m_running; }
    bool renderPreview(const QVariantList &segments, const QString &projectPath,
                       const QString &backgroundPath, const QString &path = QString());
    bool startExport(const QString &sourceMediaPath, const QString &audioPath,
                     const QString &outputPath, const QVariantList &segments = {});
    void cancel();

signals:
    void progressChanged(const QString &stage, int progress);
    void previewReady(const QString &path);
    void exported(const QString &path);
    void failed(const QString &message);

private slots:
    void onRenderFinished();
    void onMediaFinished(bool success, const QString &outputPath, const QString &error);

private:
    void fail(const QString &message);
    void clearExportPaths();

    bool m_running = false;
    QString m_renderStagingPath;
    QString m_exportDestination;
    QString m_exportStagingPath;
    QString m_exportAudioPath;
    QString m_exportSubtitlePath;
    QFutureWatcher<QVariantMap> *m_renderWatcher = nullptr;
    std::shared_ptr<QAtomicInteger<bool>> m_renderCancel;
    MediaToolService *m_mediaTools = nullptr;
};

} // namespace LAStudio
