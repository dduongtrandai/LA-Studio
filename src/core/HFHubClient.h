#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

#include <curl/curl.h>

namespace LAStudio {

class HFHubClient : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)

public:
    explicit HFHubClient(QObject *parent = nullptr);

    bool isSearching() const { return m_searching; }

    Q_INVOKABLE void searchModels(const QString &query, const QString &task = {}, bool whisperOnly = false);
    Q_INVOKABLE void downloadFile(const QString &modelId,
                                  const QString &filename,
                                  const QString &destDir);
    Q_INVOKABLE void downloadUrl(const QString &url,
                                 const QString &filename,
                                 const QString &destDir);

signals:
    void searchingChanged();
    void searchFinished(const QVariantList &models);
    void searchError(const QString &error);
    void downloadProgress(const QString &modelId,
                          const QString &filename,
                          qint64 bytesReceived,
                          qint64 bytesTotal);
    void downloadFinished(const QString &modelId, const QString &filename,
                          const QString &localPath);
    void downloadError(const QString &modelId, const QString &filename,
                       const QString &error);

private:
    static size_t writeCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
    static size_t headerCallback(char *buffer, size_t size, size_t nitems, void *userdata);
    static int progressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t ultotal, curl_off_t ulnow);

    void internalDownload(const QString &url,
                          const QString &identifier,
                          const QString &filename,
                          const QString &destDir);

    bool m_searching = false;
};

} // namespace LAStudio

