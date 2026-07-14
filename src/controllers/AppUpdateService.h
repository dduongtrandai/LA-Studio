#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqml.h>

class QNetworkAccessManager;
class QNetworkReply;

namespace LAStudio {

class DownloadManager;

class AppUpdateService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AppUpdateService is managed by AppController")

    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateInfoChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadStateChanged)
    Q_PROPERTY(bool downloaded READ downloaded NOTIFY downloadStateChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadStateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateInfoChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY updateInfoChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit AppUpdateService(DownloadManager *downloads, QObject *parent = nullptr);

    bool checking() const { return m_checking; }
    bool updateAvailable() const { return !m_latestVersion.isEmpty(); }
    bool downloading() const { return m_downloading; }
    bool downloaded() const { return !m_downloadedInstallerPath.isEmpty(); }
    qreal downloadProgress() const { return m_downloadProgress; }
    QString currentVersion() const;
    QString latestVersion() const { return m_latestVersion; }
    QString releaseUrl() const { return m_releaseUrl.toString(); }
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }

    Q_INVOKABLE void checkForUpdates(const QString &channel);
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE void installDownloadedUpdate();
    Q_INVOKABLE void clearError();

signals:
    void checkingChanged();
    void updateInfoChanged();
    void downloadStateChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void errorOccurred(const QString &message);

private slots:
    void onDownloadFinished(const QString &identifier, const QString &filename,
                            const QString &localPath, const QVariantMap &metadata);
    void onDownloadError(const QString &identifier, const QString &filename,
                         const QString &errorMsg);

private:
    void setChecking(bool checking);
    void setStatusMessage(const QString &message);
    void setErrorMessage(const QString &message);
    void resetUpdateInfo();
    void handleReleaseReply(QNetworkReply *reply, const QString &channel);
    void updateDownloadProgress();
    QString installerDownloadDir() const;
    QString existingDownloadedInstallerPath(const QString &version, const QString &filename) const;

    DownloadManager *m_downloads = nullptr;
    QNetworkAccessManager *m_network = nullptr;

    bool m_checking = false;
    bool m_downloading = false;
    qreal m_downloadProgress = 0.0;
    QString m_latestVersion;
    QString m_installerFileName;
    QString m_downloadedInstallerPath;
    QString m_statusMessage;
    QString m_errorMessage;
    QUrl m_installerUrl;
    QUrl m_releaseUrl;
};

} // namespace LAStudio
