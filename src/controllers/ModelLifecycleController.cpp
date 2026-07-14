#include "ModelLifecycleController.h"
#include "core/Logger.h"

namespace LAStudio {

ModelLifecycleController::ModelLifecycleController(ResolveCallback resolveCb,
                                                   LoadCallback loadCb,
                                                   UnloadCallback unloadCb,
                                                   QObject *parent)
    : QObject(parent)
    , m_resolveCb(resolveCb)
    , m_loadCb(loadCb)
    , m_unloadCb(unloadCb)
{
}

bool ModelLifecycleController::modelActive() const
{
    return m_state == ModelSessionState::Ready || m_state == ModelSessionState::Processing;
}

bool ModelLifecycleController::canProcess() const
{
    return m_state == ModelSessionState::Ready;
}

QString ModelLifecycleController::activeSignature() const
{
    return m_activeConfig ? m_activeConfig->signature : QString();
}

std::optional<SessionConfiguration> ModelLifecycleController::pendingConfiguration() const
{
    if (m_pendingConfig) {
        return m_pendingConfig;
    }
    return m_inFlightConfig;
}

void ModelLifecycleController::requestLoad(const QString &capabilityId, const StudioConfiguration &configuration)
{
    Logger::info(QStringLiteral("ModelLifecycleController"),
                 QStringLiteral("requestLoad for capability %1, family %2, current state: %3")
                 .arg(capabilityId)
                 .arg(configuration.familyId)
                 .arg(static_cast<int>(m_state)));

    auto resolvedOpt = m_resolveCb(configuration);
    if (!resolvedOpt) {
        Logger::error(QStringLiteral("ModelLifecycleController"), QStringLiteral("Failed to resolve configuration."));
        transitionTo(ModelSessionState::Error);
        emit errorOccurred(QStringLiteral("Failed to resolve model configuration."));
        return;
    }

    SessionConfiguration resolved = *resolvedOpt;

    if (m_state == ModelSessionState::Loading ||
        m_state == ModelSessionState::Processing ||
        m_state == ModelSessionState::Unloading)
    {
        // Enqueue as pending
        m_pendingConfig = resolved;
        m_pendingUnload = false;
        emit pendingConfigurationChanged();
        Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("Session busy. Enqueued load configuration as pending."));
        return;
    }

    if (m_state == ModelSessionState::Ready) {
        if (activeSignature() == resolved.signature) {
            // Already loaded
            Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("Configuration signature matches active signature. Ignoring load request."));
            return;
        }
        // Different configuration, trigger unload first then load
        m_pendingConfig = resolved;
        m_pendingUnload = false;
        emit pendingConfigurationChanged();
        Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("Ready with different config. Triggering unload first."));
        transitionTo(ModelSessionState::Unloading);
        m_unloadCb();
        return;
    }

    // Unconfigured, Unloaded, or Error
    m_pendingConfig = resolved;
    emit pendingConfigurationChanged();
    processPendingOperations();
}

void ModelLifecycleController::requestUnload(const QString &capabilityId)
{
    Logger::info(QStringLiteral("ModelLifecycleController"),
                 QStringLiteral("requestUnload for capability %1, current state: %2")
                 .arg(capabilityId)
                 .arg(static_cast<int>(m_state)));

    if (m_state == ModelSessionState::Processing ||
        m_state == ModelSessionState::Loading ||
        m_state == ModelSessionState::Unloading) {
        m_pendingUnload = true;
        m_pendingConfig.reset();
        emit pendingConfigurationChanged();
        Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("Session busy. Enqueued unload request as pending."));
        return;
    }

    if (m_state != ModelSessionState::Ready && !m_activeConfig && !m_inFlightConfig && !m_pendingConfig) {
        m_pendingUnload = false;
        emit pendingConfigurationChanged();
        Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("No active or pending model to unload. Ignoring unload request."));
        return;
    }

    m_pendingUnload = false;
    m_pendingConfig.reset();
    emit pendingConfigurationChanged();

    transitionTo(ModelSessionState::Unloading);
    m_unloadCb();
}

void ModelLifecycleController::requestReload(const QString &capabilityId)
{
    Logger::info(QStringLiteral("ModelLifecycleController"),
                 QStringLiteral("requestReload for capability %1, current state: %2")
                 .arg(capabilityId)
                 .arg(static_cast<int>(m_state)));

    if (m_activeConfig) {
        m_pendingConfig = m_activeConfig;
        m_pendingUnload = false;
        emit pendingConfigurationChanged();

        if (m_state == ModelSessionState::Processing ||
            m_state == ModelSessionState::Loading ||
            m_state == ModelSessionState::Unloading)
        {
            Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("Session busy. Enqueued reload request as pending."));
            return;
        }

        transitionTo(ModelSessionState::Unloading);
        m_unloadCb();
    } else {
        Logger::warning(QStringLiteral("ModelLifecycleController"), QStringLiteral("No active configuration to reload."));
    }
}

void ModelLifecycleController::onLoadSuccess()
{
    Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("onLoadSuccess"));
    if (m_inFlightConfig) {
        m_activeConfig = m_inFlightConfig;
        m_inFlightConfig.reset();
        emit pendingConfigurationChanged();
    }

    transitionTo(ModelSessionState::Ready);
    emit activeConfigurationChanged();
    emit activeSignatureChanged();

    processPendingOperations();
}

void ModelLifecycleController::onLoadError(const QString &message)
{
    Logger::error(QStringLiteral("ModelLifecycleController"), QStringLiteral("onLoadError: ") + message);
    m_inFlightConfig.reset();
    emit pendingConfigurationChanged();
    transitionTo(ModelSessionState::Error);
    emit errorOccurred(message);

    processPendingOperations();
}

void ModelLifecycleController::onUnloadFinished()
{
    Logger::info(QStringLiteral("ModelLifecycleController"), QStringLiteral("onUnloadFinished"));
    m_activeConfig.reset();
    m_inFlightConfig.reset();
    transitionTo(ModelSessionState::Unloaded);
    emit activeConfigurationChanged();
    emit activeSignatureChanged();

    processPendingOperations();
}

void ModelLifecycleController::onProcessingStateChanged(bool processing)
{
    Logger::info(QStringLiteral("ModelLifecycleController"),
                 QStringLiteral("onProcessingStateChanged: %1 (Current state: %2)")
                 .arg(processing ? "true" : "false")
                 .arg(static_cast<int>(m_state)));

    if (processing) {
        if (m_state == ModelSessionState::Ready) {
            transitionTo(ModelSessionState::Processing);
        }
    } else {
        if (m_state == ModelSessionState::Processing) {
            transitionTo(ModelSessionState::Ready);
            processPendingOperations();
        }
    }
}

void ModelLifecycleController::onEngineError(const QString &message)
{
    Logger::error(QStringLiteral("ModelLifecycleController"), QStringLiteral("onEngineError: ") + message);
    transitionTo(ModelSessionState::Error);
    emit errorOccurred(message);
}

void ModelLifecycleController::transitionTo(ModelSessionState newState)
{
    if (m_state != newState) {
        Logger::info(QStringLiteral("ModelLifecycleController"),
                     QStringLiteral("Transition state: %1 -> %2")
                     .arg(static_cast<int>(m_state))
                     .arg(static_cast<int>(newState)));
        m_state = newState;
        emit stateChanged();
    }
}

void ModelLifecycleController::processPendingOperations()
{
    if (m_state == ModelSessionState::Loading ||
        m_state == ModelSessionState::Unloading ||
        m_state == ModelSessionState::Processing)
    {
        return; // Engine busy
    }

    if (m_pendingUnload) {
        m_pendingUnload = false;
        transitionTo(ModelSessionState::Unloading);
        m_unloadCb();
        return;
    }

    if (m_pendingConfig) {
        SessionConfiguration config = *m_pendingConfig;
        m_pendingConfig.reset();
        m_inFlightConfig = config;
        emit pendingConfigurationChanged();
        transitionTo(ModelSessionState::Loading);
        m_loadCb(config);
    }
}

} // namespace LAStudio
