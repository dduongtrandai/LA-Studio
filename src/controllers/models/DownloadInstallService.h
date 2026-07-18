#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QSet>
#include <QHash>
#include <QDateTime>
#include <QtQml/qqml.h>

namespace LAStudio {

class DownloadManager;
class ModelManager;
class RuntimeManager;

class DownloadInstallService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DownloadInstallService is managed by AppController")

public:
    enum InstallState {
        NotInstalled = 0,
        Downloading = 1,
        Installing = 2,
        Installed = 3,
        Failed = 4,
        UpdateAvailable = 5
    };
    Q_ENUM(InstallState)

    explicit DownloadInstallService(DownloadManager *downloads,
                                   ModelManager *models,
                                   RuntimeManager *runtimes,
                                   QObject *parent = nullptr);
    ~DownloadInstallService() override = default;

    Q_INVOKABLE bool writeVirtualModelFiles(const QVariantMap &metadata);
    Q_INVOKABLE bool enqueueModelFile(const QVariantMap &family, const QVariantMap &requirement);
    Q_INVOKABLE bool enqueueRuntime(const QVariantMap &family,
                                    const QString &capability,
                                    const QString &familyId,
                                    const QVariantMap &runtime);
    Q_INVOKABLE bool enqueueRecommendedSetup(const QVariantMap &familyItem);

    Q_INVOKABLE int modelFileState(const QString &modelId, const QString &filename) const;
    Q_INVOKABLE int runtimeState(const QString &runtimeId, const QString &version, const QString &installedPath, const QString &assetName) const;

signals:
    void errorOccurred(const QString &msg);
    void installStatesChanged();

private slots:
    void onDownloadFinished(const QString &modelId,
                            const QString &filename,
                            const QString &localPath,
                            const QVariantMap &metadata);

private:
    static QVariantMap latestSupportedRuntime(const QVariantMap &runtimeOption);
    void scheduleModelFileUpdateCheck(const QString &modelId, const QString &filename, bool acceptRemoteAsBaseline = false) const;

    friend class TestDownloadInstallService;

    DownloadManager *m_downloads = nullptr;
    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;

    mutable QSet<QString> m_activeExtractions; // Tracks "runtimeId::version"
    mutable QSet<QString> m_activeUpdateChecks;
    mutable QHash<QString, bool> m_updateAvailable;
    mutable QHash<QString, QDateTime> m_lastUpdateChecks;
};

} // namespace LAStudio
