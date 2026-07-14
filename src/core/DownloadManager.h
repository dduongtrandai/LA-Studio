#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QtQml/qqml.h>

namespace LAStudio {

class HFHubClient;

class DownloadManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DownloadManager is managed by AppController")

    Q_PROPERTY(QVariantList activeDownloads READ activeDownloads NOTIFY activeDownloadsChanged)
    Q_PROPERTY(QVariantList allDownloads READ allDownloads NOTIFY allDownloadsChanged)

public:
    explicit DownloadManager(HFHubClient *hub, QObject *parent = nullptr);

    QVariantList activeDownloads() const;
    QVariantList allDownloads() const;
    Q_INVOKABLE bool isDownloading(const QString &identifier, const QString &filename) const;

    Q_INVOKABLE void enqueue(const QString &modelId, const QString &filename,
                             const QString &destDir, const QVariantMap &metadata = {});
    Q_INVOKABLE void enqueueUrl(const QString &url, const QString &filename,
                                const QString &destDir, const QVariantMap &metadata = {});
    Q_INVOKABLE void cancelAll();
    Q_INVOKABLE void clearCompleted();
    Q_INVOKABLE void removeDownload(const QString &identifier, const QString &filename);

signals:
    void activeDownloadsChanged();
    void allDownloadsChanged();
    void finished(const QString &identifier, const QString &filename,
                  const QString &localPath, const QVariantMap &metadata);
    void error(const QString &identifier, const QString &filename,
               const QString &errorMsg);

private slots:
    void onProgress(const QString &identifier, const QString &filename,
                    qint64 bytesReceived, qint64 bytesTotal);
    void onFinished(const QString &identifier, const QString &filename,
                    const QString &localPath);
    void onError(const QString &identifier, const QString &filename,
                 const QString &errorMsg);

private:
    struct DownloadEntry {
        QString identifier;
        QString filename;
        qint64 bytesReceived = 0;
        qint64 bytesTotal = 0;
        bool done = false;
        QString status = QStringLiteral("downloading"); // "downloading", "completed", "failed"
        QString errorMsg;
        QString localPath;
        QVariantMap metadata;
    };

    void loadHistory();
    void saveHistory() const;
    QString historyFilePath() const;
    QString makeKey(const QString &modelId, const QString &filename) const;

    HFHubClient *m_hub;
    QMap<QString, DownloadEntry> m_downloads;
    QList<QString> m_order;
};

} // namespace LAStudio

