#include "SttModelSession.h"
#include "stt/SttEngine.h"
#include "controllers/StudioConfigurationResolver.h"
#include "core/Logger.h"
#include <QFileInfo>

namespace LAStudio {

SttModelSession::SttModelSession(SttEngine *engine, QObject *parent)
    : IModelSession(parent)
    , m_engine(engine)
{
    m_lifecycle = new ModelLifecycleController(
        [this](const StudioConfiguration &config) { return resolveConfig(config); },
        [this](const SessionConfiguration &config) { performLoad(config); },
        [this]() { performUnload(); },
        this
    );

    connect(m_lifecycle, &ModelLifecycleController::stateChanged, this, [this]() {
        // Forward stateChanged
        auto current = m_lifecycle->state();
        if (current != m_lastLifecycleState) {
            m_lastLifecycleState = current;
            emit stateChanged();
        }
    });
    connect(m_lifecycle, &ModelLifecycleController::activeConfigurationChanged, this, &SttModelSession::activeConfigurationChanged);
    connect(m_lifecycle, &ModelLifecycleController::pendingConfigurationChanged, this, &SttModelSession::pendingConfigurationChanged);
    connect(m_lifecycle, &ModelLifecycleController::activeSignatureChanged, this, &SttModelSession::activeSignatureChanged);
    connect(m_lifecycle, &ModelLifecycleController::errorOccurred, this, &SttModelSession::errorOccurred);

    if (m_engine) {
        connect(m_engine, &SttEngine::stateChanged, this, &SttModelSession::onEngineStateChanged);
        connect(m_engine, &SttEngine::errorOccurred, this, [this](const QString &err) {
            m_lifecycle->onLoadError(err);
        });
    }
}

ModelSessionState SttModelSession::state() const
{
    if (!m_engine) return ModelSessionState::Unloaded;
    switch (m_engine->state()) {
    case SttEngine::Unloaded: return ModelSessionState::Unloaded;
    case SttEngine::Loading: return ModelSessionState::Loading;
    case SttEngine::Ready: return ModelSessionState::Ready;
    case SttEngine::Processing: return ModelSessionState::Processing;
    case SttEngine::Error: return ModelSessionState::Error;
    }
    return ModelSessionState::Unloaded;
}

bool SttModelSession::modelActive() const
{
    return m_engine && m_engine->isModelLoaded();
}

bool SttModelSession::canProcess() const
{
    return m_engine && m_engine->state() == SttEngine::Ready;
}

std::optional<SessionConfiguration> SttModelSession::activeConfiguration() const
{
    const QString signature = activeSignature();
    if (signature.isEmpty() || !m_loadedConfigs.contains(signature)) {
        return std::nullopt;
    }
    return m_loadedConfigs.value(signature);
}

std::optional<SessionConfiguration> SttModelSession::pendingConfiguration() const
{
    return m_lifecycle->pendingConfiguration();
}

QString SttModelSession::activeSignature() const
{
    return m_engine ? m_engine->activeSignature() : QString();
}

QList<SessionConfiguration> SttModelSession::loadedConfigurations() const
{
    return m_loadedConfigs.values();
}

void SttModelSession::requestLoad(const QString &capabilityId, const StudioConfiguration &configuration)
{
    Q_UNUSED(capabilityId);
    auto resolvedOpt = resolveConfig(configuration);
    if (!resolvedOpt || !m_engine) {
        emit errorOccurred(QStringLiteral("Failed to resolve STT configuration"));
        return;
    }
    SessionConfiguration resolved = *resolvedOpt;
    const QString runtimeId = resolved.selection.runtimeId;
    const bool useGpu = runtimeId.contains(QStringLiteral("cuda"), Qt::CaseInsensitive) ||
                        runtimeId.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive);
    m_loadedConfigs.insert(resolved.signature, resolved);
    m_engine->loadInstance(resolved, useGpu);
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void SttModelSession::requestUnload(const QString &capabilityId)
{
    Q_UNUSED(capabilityId);
    requestUnloadConfiguration(activeSignature());
}

void SttModelSession::requestUnloadConfiguration(const QString &signature)
{
    if (signature.isEmpty() || !m_engine) return;
    m_loadedConfigs.remove(signature);
    m_engine->unloadInstance(signature);
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void SttModelSession::activateConfiguration(const QString &signature)
{
    if (signature.isEmpty() || !m_engine || !m_loadedConfigs.contains(signature)) return;
    m_engine->activateInstance(signature);
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void SttModelSession::requestReload(const QString &capabilityId)
{
    Q_UNUSED(capabilityId);
    auto active = activeConfiguration();
    if (active) {
        requestLoad(active->capabilityId, active->selection);
    }
}

bool SttModelSession::usesRuntime(const QString &runtimeId, const QString &runtimeVersion) const
{
    for (const SessionConfiguration &config : loadedConfigurations()) {
        if (config.selection.runtimeId == runtimeId && config.selection.runtimeVersion == runtimeVersion) {
            return true;
        }
    }
    return false;
}

bool SttModelSession::usesModelPath(const QString &modelPath) const
{
    auto cleanPath = [](const QString &p) {
        return QFileInfo(p).absoluteFilePath().replace(QStringLiteral("\\"), QStringLiteral("/"));
    };

    const QString target = cleanPath(modelPath);

    auto checkConfig = [&](const SessionConfiguration &conf) {
        for (const QString &p : conf.resolvedModelPaths) {
            if (cleanPath(p).compare(target, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    for (const SessionConfiguration &config : loadedConfigurations()) {
        if (checkConfig(config)) {
            return true;
        }
    }
    return false;
}

void SttModelSession::onEngineStateChanged()
{
    emit stateChanged();
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
}

std::optional<SessionConfiguration> SttModelSession::resolveConfig(const StudioConfiguration &config)
{
    auto resolved = StudioConfigurationResolver::resolve(config);
    if (!resolved.isValid) {
        return std::nullopt;
    }

    SessionConfiguration out;
    out.capabilityId = config.capabilityId;
    out.selection = config;
    out.runtimePath = resolved.runtimePath;
    out.familyConfig = resolved.family;
    out.resolvedPathsByRole = resolved.resolvedPaths;
    out.signature = resolved.signature;

    // Resolve paths values
    for (const auto &val : resolved.resolvedPaths.values()) {
        out.resolvedModelPaths.append(val.toString());
    }

    return out;
}

void SttModelSession::performLoad(const SessionConfiguration &config)
{
    if (!m_engine) {
        m_lifecycle->onLoadError(QStringLiteral("STT Engine is null"));
        return;
    }

    // STT Engine needs model path (usually key "model") and runtime path
    // Let's resolve the specific model path from selection mapping
    auto resolved = StudioConfigurationResolver::resolve(config.selection);
    QString modelPath = resolved.resolvedPaths.value(QStringLiteral("model")).toString();

    const QString runtimeId = config.selection.runtimeId;
    const bool useGpu = runtimeId.contains(QStringLiteral("cuda"), Qt::CaseInsensitive) ||
                        runtimeId.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive);
    Logger::info(QStringLiteral("SttModelSession"),
                 QStringLiteral("performLoad modelPath: %1, runtimePath: %2, useGpu: %3")
                     .arg(modelPath, config.runtimePath)
                     .arg(useGpu));
    m_engine->loadModel(modelPath, useGpu, config.runtimePath);
}

void SttModelSession::performUnload()
{
    if (m_engine) {
        Logger::info(QStringLiteral("SttModelSession"), QStringLiteral("performUnload"));
        m_engine->unloadModel();
    } else {
        m_lifecycle->onUnloadFinished();
    }
}

} // namespace LAStudio
