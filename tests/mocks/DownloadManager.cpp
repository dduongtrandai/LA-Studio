#include "core/DownloadManager.h"

namespace LAStudio {

DownloadManager::DownloadManager(HFHubClient *hub, QObject *parent)
    : QObject(parent)
    , m_hub(hub)
{
}

QVariantList DownloadManager::activeDownloads() const
{
    return {};
}

QVariantList DownloadManager::allDownloads() const
{
    return {};
}

bool DownloadManager::isDownloading(const QString &, const QString &) const
{
    return false;
}

void DownloadManager::enqueue(const QString &, const QString &, const QString &, const QVariantMap &)
{
}

void DownloadManager::enqueueUrl(const QString &, const QString &, const QString &, const QVariantMap &)
{
}

void DownloadManager::cancelAll()
{
}

void DownloadManager::clearCompleted()
{
}

void DownloadManager::removeDownload(const QString &, const QString &)
{
}

void DownloadManager::loadHistory()
{
}

void DownloadManager::saveHistory() const
{
}

QString DownloadManager::historyFilePath() const
{
    return {};
}

void DownloadManager::onProgress(const QString &, const QString &, qint64, qint64) {}
void DownloadManager::onFinished(const QString &, const QString &, const QString &) {}
void DownloadManager::onError(const QString &, const QString &, const QString &) {}

QString DownloadManager::makeKey(const QString &modelId, const QString &filename) const
{
    return modelId + QStringLiteral(":") + filename;
}

} // namespace LAStudio
