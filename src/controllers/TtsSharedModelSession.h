#pragma once
#include "IModelSession.h"
#include "ModelLifecycleController.h"
#include <QHash>

namespace LAStudio {

class TtsEngine;

class TtsSharedModelSession : public IModelSession {
    Q_OBJECT
public:
    explicit TtsSharedModelSession(TtsEngine *engine, QObject *parent = nullptr);
    ~TtsSharedModelSession() override = default;

    ModelSessionState state() const override;
    bool modelActive() const override;
    bool canProcess() const override;

    std::optional<SessionConfiguration> activeConfiguration() const override;
    std::optional<SessionConfiguration> pendingConfiguration() const override;
    QList<SessionConfiguration> loadedConfigurations() const override;
    QString activeSignature() const override;

    void requestLoad(const QString &capabilityId,
                     const StudioConfiguration &configuration) override;
    void requestUnload(const QString &capabilityId) override;
    void requestUnloadConfiguration(const QString &signature) override;
    void activateConfiguration(const QString &signature) override;
    void requestReload(const QString &capabilityId) override;

    bool usesRuntime(const QString &runtimeId,
                     const QString &runtimeVersion) const override;
    bool usesModelPath(const QString &modelPath) const override;

    // Owner tracking
    QString activeOwner() const { return m_activeOwner; }
    QString pendingOwner() const { return m_pendingOwner; }
    bool isOwnerActive(const QString &capabilityId) const;
    bool isOwnerPending(const QString &capabilityId) const;

private slots:
    void onEngineStateChanged();

private:
    std::optional<SessionConfiguration> resolveConfig(const StudioConfiguration &config);
    void performLoad(const SessionConfiguration &config);
    void performUnload();

    TtsEngine *m_engine = nullptr;
    ModelLifecycleController *m_lifecycle = nullptr;
    QHash<QString, SessionConfiguration> m_loadedConfigs;
    QHash<QString, QString> m_ownerBySignature;
    QString m_activeOwner;
    QString m_pendingOwner;
    ModelSessionState m_lastLifecycleState = ModelSessionState::Unconfigured;
};

} // namespace LAStudio
