#include "TtsSharedModelSession.h"
#include "tts/TtsEngine.h"
#include "controllers/StudioConfigurationResolver.h"
#include "core/Logger.h"
#include <QFileInfo>

namespace LAStudio {

TtsSharedModelSession::TtsSharedModelSession(TtsEngine *engine, QObject *parent)
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
        auto current = m_lifecycle->state();
        if (current != m_lastLifecycleState) {
            m_lastLifecycleState = current;
            emit stateChanged();
        }
    });
    connect(m_lifecycle, &ModelLifecycleController::activeConfigurationChanged, this, &TtsSharedModelSession::activeConfigurationChanged);
    connect(m_lifecycle, &ModelLifecycleController::pendingConfigurationChanged, this, &TtsSharedModelSession::pendingConfigurationChanged);
    connect(m_lifecycle, &ModelLifecycleController::activeSignatureChanged, this, &TtsSharedModelSession::activeSignatureChanged);
    connect(m_lifecycle, &ModelLifecycleController::errorOccurred, this, &TtsSharedModelSession::errorOccurred);

    if (m_engine) {
        connect(m_engine, &TtsEngine::stateChanged, this, &TtsSharedModelSession::onEngineStateChanged);
        connect(m_engine, &TtsEngine::errorOccurred, this, [this](const QString &err) {
            if (m_lifecycle->state() == ModelSessionState::Loading) {
                m_lifecycle->onLoadError(err);
            } else if (m_lifecycle->state() == ModelSessionState::Processing) {
                m_lifecycle->onProcessingStateChanged(false);
            }
        });
    }
}

ModelSessionState TtsSharedModelSession::state() const
{
    if (!m_engine) return ModelSessionState::Unloaded;
    switch (m_engine->state()) {
    case TtsEngine::Unloaded: return ModelSessionState::Unloaded;
    case TtsEngine::Loading: return ModelSessionState::Loading;
    case TtsEngine::Ready: return ModelSessionState::Ready;
    case TtsEngine::Processing: return ModelSessionState::Processing;
    case TtsEngine::Error: return ModelSessionState::Error;
    }
    return ModelSessionState::Unloaded;
}

bool TtsSharedModelSession::modelActive() const
{
    return m_engine && m_engine->isModelLoaded();
}

bool TtsSharedModelSession::canProcess() const
{
    return m_engine && m_engine->state() == TtsEngine::Ready;
}

std::optional<SessionConfiguration> TtsSharedModelSession::activeConfiguration() const
{
    const QString signature = activeSignature();
    if (signature.isEmpty() || !m_loadedConfigs.contains(signature)) {
        return std::nullopt;
    }
    SessionConfiguration conf = m_loadedConfigs.value(signature);
    conf.capabilityId = m_ownerBySignature.value(signature, m_activeOwner);
    return conf;
}

std::optional<SessionConfiguration> TtsSharedModelSession::pendingConfiguration() const
{
    auto conf = m_lifecycle->pendingConfiguration();
    if (conf) {
        conf->capabilityId = m_pendingOwner.isEmpty() ? m_activeOwner : m_pendingOwner;
    }
    return conf;
}

QString TtsSharedModelSession::activeSignature() const
{
    return m_engine ? m_engine->activeSignature() : QString();
}

QList<SessionConfiguration> TtsSharedModelSession::loadedConfigurations() const
{
    QList<SessionConfiguration> out;
    for (auto it = m_loadedConfigs.cbegin(); it != m_loadedConfigs.cend(); ++it) {
        SessionConfiguration conf = it.value();
        conf.capabilityId = m_ownerBySignature.value(it.key(), conf.capabilityId);
        out.append(conf);
    }
    return out;
}

bool TtsSharedModelSession::isOwnerActive(const QString &capabilityId) const
{
    return !capabilityId.isEmpty() && capabilityId == m_activeOwner;
}

bool TtsSharedModelSession::isOwnerPending(const QString &capabilityId) const
{
    return !capabilityId.isEmpty() && capabilityId == m_pendingOwner;
}

void TtsSharedModelSession::requestLoad(const QString &capabilityId, const StudioConfiguration &configuration)
{
    Logger::info(QStringLiteral("TtsSharedModelSession"),
                 QStringLiteral("requestLoad from capability: %1").arg(capabilityId));

    auto resolvedOpt = resolveConfig(configuration);
    if (!resolvedOpt) {
        emit errorOccurred(QStringLiteral("Failed to resolve configuration"));
        return;
    }

    SessionConfiguration resolved = *resolvedOpt;
    if (!m_engine) {
        emit errorOccurred(QStringLiteral("TTS Engine is null"));
        return;
    }

    const QStringList loadedSignatures = m_loadedConfigs.keys();
    for (const QString &signature : loadedSignatures) {
        if (signature == resolved.signature) {
            continue;
        }
        Logger::info(QStringLiteral("TtsSharedModelSession"),
                     QStringLiteral("Unloading previous TTS configuration before loading: %1").arg(resolved.signature));
        m_loadedConfigs.remove(signature);
        m_ownerBySignature.remove(signature);
        m_engine->unloadInstance(signature);
    }

    m_pendingOwner = capabilityId;
    m_activeOwner = capabilityId;
    m_loadedConfigs.insert(resolved.signature, resolved);
    m_ownerBySignature.insert(resolved.signature, capabilityId);
    m_engine->loadInstance(resolved);
    m_pendingOwner.clear();
    emit pendingConfigurationChanged();
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void TtsSharedModelSession::requestUnload(const QString &capabilityId)
{
    Q_UNUSED(capabilityId);
    requestUnloadConfiguration(activeSignature());
}

void TtsSharedModelSession::requestUnloadConfiguration(const QString &signature)
{
    if (signature.isEmpty() || !m_engine) return;
    m_loadedConfigs.remove(signature);
    m_ownerBySignature.remove(signature);
    m_engine->unloadInstance(signature);
    m_activeOwner = activeConfiguration() ? activeConfiguration()->capabilityId : QString();
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void TtsSharedModelSession::activateConfiguration(const QString &signature)
{
    if (signature.isEmpty() || !m_engine || !m_loadedConfigs.contains(signature)) return;
    m_engine->activateInstance(signature);
    m_activeOwner = m_ownerBySignature.value(signature, m_loadedConfigs.value(signature).capabilityId);
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void TtsSharedModelSession::requestReload(const QString &capabilityId)
{
    if (m_activeOwner == capabilityId) {
        auto active = activeConfiguration();
        if (active && m_engine) {
            m_engine->loadInstance(*active);
        }
    }
}

bool TtsSharedModelSession::usesRuntime(const QString &runtimeId, const QString &runtimeVersion) const
{
    for (const SessionConfiguration &config : loadedConfigurations()) {
        if (config.selection.runtimeId == runtimeId && config.selection.runtimeVersion == runtimeVersion) {
            return true;
        }
    }
    return false;
}

bool TtsSharedModelSession::usesModelPath(const QString &modelPath) const
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

void TtsSharedModelSession::onEngineStateChanged()
{
    emit stateChanged();
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
}

std::optional<SessionConfiguration> TtsSharedModelSession::resolveConfig(const StudioConfiguration &config)
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

    for (const auto &val : resolved.resolvedPaths.values()) {
        out.resolvedModelPaths.append(val.toString());
    }

    return out;
}

void TtsSharedModelSession::performLoad(const SessionConfiguration &config)
{
    if (!m_engine) {
        m_lifecycle->onLoadError(QStringLiteral("TTS Engine is null"));
        return;
    }

    auto resolved = StudioConfigurationResolver::resolve(config.selection);
    if (!resolved.isValid) {
        m_lifecycle->onLoadError(QStringLiteral("Failed to resolve configuration during load"));
        return;
    }

    m_engine->clearLastSamples();
    m_engine->setFamilyConfig(resolved.family);
    m_engine->setRuntimePath(config.runtimePath);
    m_engine->loadModel(resolved.resolvedPaths, config.runtimePath);
}

void TtsSharedModelSession::performUnload()
{
    if (m_engine) {
        m_engine->unloadVoice();
    } else {
        m_lifecycle->onUnloadFinished();
    }
}

} // namespace LAStudio
