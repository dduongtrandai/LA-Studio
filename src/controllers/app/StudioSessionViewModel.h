#pragma once
#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqml.h>
#include "controllers/app/IStudioAction.h"
#include "IModelSession.h"
#include "core/CapabilityFamilyModel.h"
#include "core/StudioSelectionRepository.h"

namespace LAStudio {

class StudioSessionViewModel : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(StudioPageController)

    Q_PROPERTY(QString capabilityId READ capabilityId WRITE setCapabilityId NOTIFY capabilityIdChanged)
    Q_PROPERTY(bool autoLoadOnSync READ autoLoadOnSync WRITE setAutoLoadOnSync NOTIFY autoLoadOnSyncChanged)
    Q_PROPERTY(QString selectedFamilyId READ selectedFamilyId NOTIFY selectionChanged)
    Q_PROPERTY(QString runtimeId READ runtimeId NOTIFY selectionChanged)
    Q_PROPERTY(QString runtimeVersion READ runtimeVersion NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedFiles READ selectedFiles NOTIFY selectionChanged)
    Q_PROPERTY(QObject* familiesModel READ familiesModel CONSTANT)
    Q_PROPERTY(bool selectionCommitted READ selectionCommitted NOTIFY selectionChanged)
    Q_PROPERTY(bool studioReady READ studioReady NOTIFY stateChanged)
    Q_PROPERTY(StudioState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString statusDetail READ statusDetail NOTIFY stateChanged)
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY stateChanged)
    Q_PROPERTY(QString estimatedRamUsage READ estimatedRamUsage NOTIFY stateChanged)
    Q_PROPERTY(QString estimatedVramUsage READ estimatedVramUsage NOTIFY stateChanged)
    Q_PROPERTY(QString loadedModelName READ loadedModelName NOTIFY stateChanged)
    Q_PROPERTY(QVariantList loadedModelFiles READ loadedModelFiles NOTIFY stateChanged)
    Q_PROPERTY(QVariantList loadedModels READ loadedModels NOTIFY stateChanged)
    Q_PROPERTY(QString activeModelId READ activeModelId NOTIFY stateChanged)
    Q_PROPERTY(qint64 inferenceElapsedMs READ inferenceElapsedMs NOTIFY stateChanged)
    Q_PROPERTY(QString inferenceElapsedText READ inferenceElapsedText NOTIFY stateChanged)

    Q_PROPERTY(bool modelActive READ modelActive NOTIFY stateChanged)
    Q_PROPERTY(bool canProcess READ canProcess NOTIFY stateChanged)
    Q_PROPERTY(QString activeSignature READ activeSignature NOTIFY stateChanged)
    Q_PROPERTY(QString pendingSignature READ pendingSignature NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)

    // Legacy / UI Helpers
    Q_PROPERTY(QString studioHeaderTitle READ studioHeaderTitle NOTIFY stateChanged)
    Q_PROPERTY(QString modalSelectionTitle READ modalSelectionTitle NOTIFY stateChanged)
    Q_PROPERTY(QString modalSelectionValue READ modalSelectionValue NOTIFY stateChanged)
    Q_PROPERTY(QString modalSelectionDetail READ modalSelectionDetail NOTIFY stateChanged)
    Q_PROPERTY(QString runtimeDisplayText READ runtimeDisplayText NOTIFY stateChanged)
    Q_PROPERTY(QVariantList families READ families NOTIFY capabilityIdChanged)

public:
    explicit StudioSessionViewModel(QObject *parent = nullptr);
    ~StudioSessionViewModel() override = default;

    // Getters / Setters
    QString capabilityId() const { return m_capabilityId; }
    void setCapabilityId(const QString &id);
    bool autoLoadOnSync() const { return m_autoLoadOnSync; }
    void setAutoLoadOnSync(bool enabled);

    QString selectedFamilyId() const;
    QString runtimeId() const;
    QString runtimeVersion() const;
    QVariantMap selectedFiles() const;
    QObject* familiesModel() const { return m_familiesModel; }
    bool selectionCommitted() const { return m_selectionCommitted; }
    bool studioReady() const;
    StudioState state() const;
    QString statusText() const;
    QString statusDetail() const;
    double cpuUsage() const;
    QString estimatedRamUsage() const;
    QString estimatedVramUsage() const;
    QString loadedModelName() const;
    QVariantList loadedModelFiles() const;
    QVariantList loadedModels() const;
    QString activeModelId() const;
    qint64 inferenceElapsedMs() const;
    QString inferenceElapsedText() const;

    bool modelActive() const;
    bool canProcess() const;
    QString activeSignature() const;
    QString pendingSignature() const;
    QString lastError() const;

    QString studioHeaderTitle() const;
    QString modalSelectionTitle() const;
    QString modalSelectionValue() const;
    QString modalSelectionDetail() const;
    QString runtimeDisplayText() const;
    QVariantList families() const;

    // Invokable Commands for QML
    Q_INVOKABLE void openConfiguration(const QString &familyId);
    Q_INVOKABLE void selectFamily(const QString &familyId);
    Q_INVOKABLE void selectRuntime(const QString &runtimeId, const QString &version);
    Q_INVOKABLE void commitSelection();
    Q_INVOKABLE void loadSelectedConfiguration();
    Q_INVOKABLE void unload();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void activateLoadedModel(const QString &modelId);
    Q_INVOKABLE void unloadLoadedModel(const QString &modelId);

    // Legacy StudioContext alignment helpers
    Q_INVOKABLE QString getStudioContextFamilyId() const { return selectedFamilyId(); }
    Q_INVOKABLE QString getStudioContextRuntimeId() const { return runtimeId(); }
    Q_INVOKABLE QString getStudioContextRuntimeVersion() const { return runtimeVersion(); }

    // Legacy Controller methods fallback
    Q_INVOKABLE void syncSelectionFromSettings();
    Q_INVOKABLE void commitConfigurationSelection(const QString &familyId, const QString &runtimeId, const QString &runtimeVersion, const QVariantMap &selectedFiles = QVariantMap());
    Q_INVOKABLE void saveConfigurationSelection(const QString &familyId, const QString &runtimeId, const QString &runtimeVersion, const QVariantMap &selectedFiles = QVariantMap());
    Q_INVOKABLE void autoLoadIfReady();
    Q_INVOKABLE void reloadRequested() { reload(); }
    Q_INVOKABLE void ejectRequested() { unload(); }
    Q_INVOKABLE void modelSwitchRequested(const QString &nextFamilyId) { selectFamily(nextFamilyId); commitSelection(); }
    Q_INVOKABLE void runtimeSwitchRequested(const QString &nextRuntimeId) { selectRuntime(nextRuntimeId, QString()); commitSelection(); }

signals:
    void capabilityIdChanged();
    void autoLoadOnSyncChanged();
    void selectionChanged();
    void stateChanged();
    
    // Legacy support signals
    void studioContextChanged(const QString &familyId, const QString &runtimeId, const QString &runtimeVersion);
    void configurationDialogClosed();
    void configurationGalleryRequestReset();

private slots:
    void onActionStateChanged();
    void onActionConfigurationChanged();

private:
    void commitSelectionInternal(bool loadAfterCommit);
    void initializeAction();
    QVariantMap getFamilyConfig(const QString &familyId) const;
    std::optional<SessionConfiguration> activeSessionConfiguration() const;
    QVariantList filesForConfiguration(const SessionConfiguration &config) const;
    QVariantMap modelListItemForConfiguration(const SessionConfiguration &config, bool active) const;
    void syncInferenceTimer();
    static QString formatElapsed(qint64 elapsedMs);

    QString m_capabilityId;
    CapabilityFamilyModel* m_familiesModel = nullptr;
    IStudioAction* m_action = nullptr;
    StudioSelectionRepository* m_repository = nullptr;
    
    // In-progress pending selection (pre-commit)
    QString m_pendingFamilyId;
    QString m_pendingRuntimeId;
    QString m_pendingRuntimeVersion;
    QVariantMap m_pendingFiles;

    bool m_selectionCommitted = false;
    bool m_autoLoadOnSync = false;
    QElapsedTimer m_inferenceTimer;
    QTimer *m_inferenceUiTimer = nullptr;
    qint64 m_lastInferenceElapsedMs = 0;
};

} // namespace LAStudio
