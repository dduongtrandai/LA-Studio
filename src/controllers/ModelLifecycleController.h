#pragma once
#include <QObject>
#include <functional>
#include <optional>
#include "IModelSession.h"

namespace LAStudio {

class ModelLifecycleController : public QObject {
    Q_OBJECT
public:
    using ResolveCallback = std::function<std::optional<SessionConfiguration>(const StudioConfiguration&)>;
    using LoadCallback = std::function<void(const SessionConfiguration&)>;
    using UnloadCallback = std::function<void()>;

    explicit ModelLifecycleController(ResolveCallback resolveCb,
                                      LoadCallback loadCb,
                                      UnloadCallback unloadCb,
                                      QObject *parent = nullptr);

    ModelSessionState state() const { return m_state; }
    bool modelActive() const;
    bool canProcess() const;

    std::optional<SessionConfiguration> activeConfiguration() const { return m_activeConfig; }
    std::optional<SessionConfiguration> pendingConfiguration() const;
    QString activeSignature() const;

    void requestLoad(const QString &capabilityId, const StudioConfiguration &configuration);
    void requestUnload(const QString &capabilityId);
    void requestReload(const QString &capabilityId);

    // Callbacks for engine adapters to report status updates
    void onLoadSuccess();
    void onLoadError(const QString &message);
    void onUnloadFinished();
    void onProcessingStateChanged(bool processing);
    void onEngineError(const QString &message);

signals:
    void stateChanged();
    void activeConfigurationChanged();
    void pendingConfigurationChanged();
    void activeSignatureChanged();
    void errorOccurred(const QString &message);

private:
    void transitionTo(ModelSessionState newState);
    void processPendingOperations();

    ResolveCallback m_resolveCb;
    LoadCallback m_loadCb;
    UnloadCallback m_unloadCb;

    ModelSessionState m_state = ModelSessionState::Unconfigured;
    std::optional<SessionConfiguration> m_activeConfig;
    std::optional<SessionConfiguration> m_inFlightConfig;
    std::optional<SessionConfiguration> m_pendingConfig;
    bool m_pendingUnload = false;
};

} // namespace LAStudio
