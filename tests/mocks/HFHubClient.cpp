#include "core/HFHubClient.h"

namespace LAStudio {

HFHubClient::HFHubClient(QObject *parent)
    : QObject(parent)
{
}

void HFHubClient::searchModels(const QString &, const QString &, bool)
{
}

void HFHubClient::downloadFile(const QString &, const QString &, const QString &)
{
}

void HFHubClient::downloadUrl(const QString &, const QString &, const QString &)
{
}

size_t HFHubClient::writeCallback(char *, size_t, size_t, void *)
{
    return 0;
}

size_t HFHubClient::headerCallback(char *, size_t, size_t, void *)
{
    return 0;
}

int HFHubClient::progressCallback(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return 0;
}

void HFHubClient::internalDownload(const QString &, const QString &, const QString &, const QString &)
{
}

} // namespace LAStudio
