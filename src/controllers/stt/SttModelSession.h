#pragma once
#include "IModelSession.h"
#include "ModelLifecycleController.h"
#include <QHash>

namespace LAStudio {

class SttEngine;

class SttModelSession : public IModelSession {
    Q_OBJECT
public:
    explicit SttModelSession(SttEngine *engine, QObject *parent = nullptr);
    ~SttModelSession() override = default;

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

private slots:
    void onEngineStateChanged();

private:
    std::optional<SessionConfiguration> resolveConfig(const StudioConfiguration &config);
    void performLoad(const SessionConfiguration &config);
    void performUnload();

    SttEngine *m_engine = nullptr;
    ModelLifecycleController *m_lifecycle = nullptr;
    QHash<QString, SessionConfiguration> m_loadedConfigs;
    ModelSessionState m_lastLifecycleState = ModelSessionState::Unconfigured;
};

} // namespace LAStudio
