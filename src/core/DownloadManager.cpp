#include "DownloadManager.h"
#include "HFHubClient.h"
#include "Logger.h"
#include "PathUtils.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace LAStudio {

DownloadManager::DownloadManager(HFHubClient *hub, QObject *parent)
    : QObject(parent)
    , m_hub(hub)
{
    connect(m_hub, &HFHubClient::downloadProgress,
            this, &DownloadManager::onProgress);
    connect(m_hub, &HFHubClient::downloadFinished,
            this, &DownloadManager::onFinished);
    connect(m_hub, &HFHubClient::downloadError,
            this, &DownloadManager::onError);

    loadHistory();
}

QString DownloadManager::makeKey(const QString &modelId, const QString &filename) const
{
    return modelId + QStringLiteral("::") + filename;
}

QVariantList DownloadManager::activeDownloads() const
{
    QVariantList list;
    for (auto it = m_downloads.cbegin(); it != m_downloads.cend(); ++it) {
        if (!it->done) {
            QVariantMap m;
            m[QStringLiteral("identifier")]      = it->identifier;
            m[QStringLiteral("filename")]       = it->filename;
            m[QStringLiteral("bytesReceived")]  = it->bytesReceived;
            m[QStringLiteral("bytesTotal")]     = it->bytesTotal;
            m[QStringLiteral("metadata")]       = it->metadata;
            list.append(m);
        }
    }
    return list;
}

QVariantList DownloadManager::allDownloads() const
{
    QVariantList list;
    for (const QString &key : m_order) {
        if (!m_downloads.contains(key)) continue;
        const auto &it = m_downloads[key];
        QVariantMap m;
        m[QStringLiteral("identifier")]      = it.identifier;
        m[QStringLiteral("filename")]       = it.filename;
        m[QStringLiteral("bytesReceived")]  = it.bytesReceived;
        m[QStringLiteral("bytesTotal")]     = it.bytesTotal;
        m[QStringLiteral("done")]           = it.done;
        m[QStringLiteral("status")]         = it.status;
        m[QStringLiteral("errorMsg")]       = it.errorMsg;
        m[QStringLiteral("localPath")]      = it.localPath;
        m[QStringLiteral("metadata")]       = it.metadata;
        list.append(m);
    }
    return list;
}

bool DownloadManager::isDownloading(const QString &identifier, const QString &filename) const
{
    QString key = makeKey(identifier, filename);
    return m_downloads.contains(key) && !m_downloads[key].done;
}

void DownloadManager::enqueue(const QString &modelId, const QString &filename,
                               const QString &destDir, const QVariantMap &metadata)
{
    QString key = makeKey(modelId, filename);
    if (m_downloads.contains(key) && !m_downloads[key].done)
        return; // already downloading

    DownloadEntry e;
    e.identifier = modelId;
    e.filename = filename;
    e.status = QStringLiteral("downloading");
    e.metadata = metadata;
    m_downloads[key] = e;

    m_order.removeAll(key);
    m_order.prepend(key);

    saveHistory();

    Logger::info(QStringLiteral("Download"), QStringLiteral("Enqueued HuggingFace file download: %1/%2 to %3").arg(modelId, filename, destDir));

    emit activeDownloadsChanged();
    emit allDownloadsChanged();
    m_hub->downloadFile(modelId, filename, destDir);
}

void DownloadManager::enqueueUrl(const QString &url, const QString &filename,
                                  const QString &destDir, const QVariantMap &metadata)
{
    QString key = makeKey(url, filename);
    if (m_downloads.contains(key) && !m_downloads[key].done)
        return; // already downloading

    DownloadEntry e;
    e.identifier = url;
    e.filename = filename;
    e.status = QStringLiteral("downloading");
    e.metadata = metadata;
    m_downloads[key] = e;

    m_order.removeAll(key);
    m_order.prepend(key);

    saveHistory();

    Logger::info(QStringLiteral("Download"), QStringLiteral("Enqueued URL download: %1 to %2").arg(url, destDir));

    emit activeDownloadsChanged();
    emit allDownloadsChanged();
    m_hub->downloadUrl(url, filename, destDir);
}

void DownloadManager::cancelAll()
{
    m_downloads.clear();
    m_order.clear();
    saveHistory();
    emit activeDownloadsChanged();
    emit allDownloadsChanged();
}

void DownloadManager::clearCompleted()
{
    for (auto it = m_downloads.begin(); it != m_downloads.end(); ) {
        if (it->done) {
            m_order.removeAll(it.key());
            it = m_downloads.erase(it);
        } else {
            ++it;
        }
    }
    saveHistory();
    emit activeDownloadsChanged();
    emit allDownloadsChanged();
}

void DownloadManager::removeDownload(const QString &identifier, const QString &filename)
{
    QString key = makeKey(identifier, filename);
    if (m_downloads.contains(key)) {
        m_downloads.remove(key);
        m_order.removeAll(key);
        saveHistory();
        emit activeDownloadsChanged();
        emit allDownloadsChanged();
    }
}

void DownloadManager::onProgress(const QString &identifier, const QString &filename,
                                  qint64 bytesReceived, qint64 bytesTotal)
{
    QString key = makeKey(identifier, filename);
    if (!m_downloads.contains(key)) return;

    m_downloads[key].bytesReceived = bytesReceived;
    m_downloads[key].bytesTotal = bytesTotal;
    emit activeDownloadsChanged();
    emit allDownloadsChanged();
}

void DownloadManager::onFinished(const QString &identifier, const QString &filename,
                                  const QString &localPath)
{
    QString key = makeKey(identifier, filename);
    QVariantMap metadata;
    if (m_downloads.contains(key)) {
        m_downloads[key].done = true;
        m_downloads[key].status = QStringLiteral("completed");
        m_downloads[key].localPath = localPath;
        metadata = m_downloads[key].metadata;
    }

    Logger::info(QStringLiteral("Download"), QStringLiteral("Download completed successfully: %1 (Saved to: %2)").arg(filename, localPath));

    saveHistory();
    emit activeDownloadsChanged();
    emit allDownloadsChanged();
    emit finished(identifier, filename, localPath, metadata);
}

void DownloadManager::onError(const QString &identifier, const QString &filename,
                               const QString &errorMsg)
{
    QString key = makeKey(identifier, filename);
    if (m_downloads.contains(key)) {
        m_downloads[key].done = true;
        m_downloads[key].status = QStringLiteral("failed");
        m_downloads[key].errorMsg = errorMsg;
    }

    Logger::error(QStringLiteral("Download"), QStringLiteral("Download failed for %1: %2").arg(filename, errorMsg));

    saveHistory();
    emit activeDownloadsChanged();
    emit allDownloadsChanged();
    emit error(identifier, filename, errorMsg);
}

QString DownloadManager::historyFilePath() const
{
    return PathUtils::dataDir() + QStringLiteral("/downloads_history.json");
}

void DownloadManager::loadHistory()
{
    QFile file(historyFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return;
    }
    QJsonObject root = doc.object();
    QJsonArray orderArr = root.value(QStringLiteral("order")).toArray();
    QJsonObject downloadsObj = root.value(QStringLiteral("downloads")).toObject();

    m_downloads.clear();
    m_order.clear();

    for (const QJsonValue &val : orderArr) {
        m_order.append(val.toString());
    }

    for (auto it = downloadsObj.begin(); it != downloadsObj.end(); ++it) {
        QString key = it.key();
        QJsonObject obj = it.value().toObject();
        DownloadEntry e;
        e.identifier = obj.value(QStringLiteral("identifier")).toString();
        e.filename = obj.value(QStringLiteral("filename")).toString();
        e.bytesReceived = obj.value(QStringLiteral("bytesReceived")).toVariant().toLongLong();
        e.bytesTotal = obj.value(QStringLiteral("bytesTotal")).toVariant().toLongLong();
        e.done = obj.value(QStringLiteral("done")).toBool();
        e.status = obj.value(QStringLiteral("status")).toString();
        e.errorMsg = obj.value(QStringLiteral("errorMsg")).toString();
        e.localPath = obj.value(QStringLiteral("localPath")).toString();
        e.metadata = obj.value(QStringLiteral("metadata")).toObject().toVariantMap();

        // Mark any interrupted downloads as failed
        if (!e.done && e.status == QStringLiteral("downloading")) {
            e.done = true;
            e.status = QStringLiteral("failed");
            e.errorMsg = QStringLiteral("Interrupted");
        }

        m_downloads.insert(key, e);
    }
}

void DownloadManager::saveHistory() const
{
    QString path = historyFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error(QStringLiteral("Download"), "Failed to write downloads history file: " + path);
        return;
    }

    QJsonObject root;
    QJsonArray orderArr;
    for (const QString &key : m_order) {
        orderArr.append(key);
    }
    root.insert(QStringLiteral("order"), orderArr);

    QJsonObject downloadsObj;
    for (auto it = m_downloads.cbegin(); it != m_downloads.cend(); ++it) {
        QJsonObject obj;
        obj.insert(QStringLiteral("identifier"), it->identifier);
        obj.insert(QStringLiteral("filename"), it->filename);
        obj.insert(QStringLiteral("bytesReceived"), it->bytesReceived);
        obj.insert(QStringLiteral("bytesTotal"), it->bytesTotal);
        obj.insert(QStringLiteral("done"), it->done);
        obj.insert(QStringLiteral("status"), it->status);
        obj.insert(QStringLiteral("errorMsg"), it->errorMsg);
        obj.insert(QStringLiteral("localPath"), it->localPath);
        obj.insert(QStringLiteral("metadata"), QJsonObject::fromVariantMap(it->metadata));
        downloadsObj.insert(it.key(), obj);
    }
    root.insert(QStringLiteral("downloads"), downloadsObj);

    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();
}

} // namespace LAStudio

