#include "LlmChatModelSession.h"
#include "llm/LlmChatEngine.h"
#include "controllers/models/StudioConfigurationResolver.h"
#include <QFileInfo>

namespace LAStudio {

LlmChatModelSession::LlmChatModelSession(LlmChatEngine *engine, QObject *parent)
    : IModelSession(parent), m_engine(engine)
{
    if (m_engine) {
        connect(m_engine, &LlmChatEngine::stateChanged, this, &LlmChatModelSession::stateChanged);
        connect(m_engine, &LlmChatEngine::modelLoadedChanged, this, &LlmChatModelSession::activeConfigurationChanged);
        connect(m_engine, &LlmChatEngine::errorOccurred, this, [this](const QString &message) {
            m_error = message; emit errorOccurred(message); emit stateChanged();
        });
    }
}

ModelSessionState LlmChatModelSession::state() const
{
    if (!m_error.isEmpty()) return ModelSessionState::Error;
    if (!m_engine) return ModelSessionState::Unloaded;
    switch (m_engine->state()) {
    case LlmChatEngine::Unloaded: return ModelSessionState::Unloaded;
    case LlmChatEngine::Loading: return ModelSessionState::Loading;
    case LlmChatEngine::Ready: return ModelSessionState::Ready;
    case LlmChatEngine::Processing: return ModelSessionState::Processing;
    case LlmChatEngine::Error: return ModelSessionState::Error;
    }
    return ModelSessionState::Unloaded;
}
bool LlmChatModelSession::modelActive() const { return m_engine && m_engine->isModelLoaded(); }
bool LlmChatModelSession::canProcess() const { return state() == ModelSessionState::Ready; }
std::optional<SessionConfiguration> LlmChatModelSession::activeConfiguration() const
{
    return m_loaded.contains(m_activeSignature)
        ? std::optional<SessionConfiguration>(m_loaded.value(m_activeSignature)) : std::nullopt;
}
std::optional<SessionConfiguration> LlmChatModelSession::pendingConfiguration() const { return std::nullopt; }
QList<SessionConfiguration> LlmChatModelSession::loadedConfigurations() const { return m_loaded.values(); }
QString LlmChatModelSession::activeSignature() const { return m_activeSignature; }

void LlmChatModelSession::requestLoad(const QString &, const StudioConfiguration &configuration)
{
    auto resolved = resolveConfig(configuration);
    if (!resolved || !m_engine) {
        m_error = QStringLiteral("Failed to resolve LLM configuration.");
        emit errorOccurred(m_error); emit stateChanged(); return;
    }
    m_error.clear();
    m_loaded.insert(resolved->signature, *resolved);
    m_activeSignature = resolved->signature;
    const QString modelPath = resolved->resolvedPathsByRole.value(QStringLiteral("model")).toString();
    const bool useGpu = configuration.runtimeId.contains(QStringLiteral("cuda"), Qt::CaseInsensitive)
        || configuration.runtimeId.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive)
        || configuration.runtimeId.contains(QStringLiteral("hip"), Qt::CaseInsensitive)
        || configuration.runtimeId.contains(QStringLiteral("sycl"), Qt::CaseInsensitive)
        || configuration.runtimeId.contains(QStringLiteral("openvino"), Qt::CaseInsensitive);
    m_engine->load(resolved->runtimePath, modelPath, useGpu);
    emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged();
}
void LlmChatModelSession::requestUnload(const QString &) { requestUnloadConfiguration(m_activeSignature); }
void LlmChatModelSession::requestUnloadConfiguration(const QString &signature)
{
    if (signature.isEmpty()) return;
    m_loaded.remove(signature);
    if (signature == m_activeSignature && m_engine) { m_engine->unload(); m_activeSignature.clear(); }
    emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged();
}
void LlmChatModelSession::activateConfiguration(const QString &signature)
{
    if (!m_loaded.contains(signature) || !m_engine) return;
    const auto config = m_loaded.value(signature);
    m_activeSignature = signature;
    const QString modelPath = config.resolvedPathsByRole.value(QStringLiteral("model")).toString();
    m_engine->load(config.runtimePath, modelPath, config.selection.runtimeId.contains(QStringLiteral("cuda"), Qt::CaseInsensitive));
    emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged();
}
void LlmChatModelSession::requestReload(const QString &) { if (auto active = activeConfiguration()) requestLoad(active->capabilityId, active->selection); }
bool LlmChatModelSession::usesRuntime(const QString &id, const QString &version) const
{
    for (const auto &config : m_loaded) if (config.selection.runtimeId == id && (version.isEmpty() || config.selection.runtimeVersion == version)) return true;
    return false;
}
bool LlmChatModelSession::usesModelPath(const QString &path) const
{
    const QString target = QFileInfo(path).absoluteFilePath();
    for (const auto &config : m_loaded) for (const QString &modelPath : config.resolvedModelPaths)
        if (QFileInfo(modelPath).absoluteFilePath().compare(target, Qt::CaseInsensitive) == 0) return true;
    return false;
}
std::optional<SessionConfiguration> LlmChatModelSession::resolveConfig(const StudioConfiguration &configuration) const
{
    const ResolvedConfiguration resolved = StudioConfigurationResolver::resolve(configuration);
    if (!resolved.isValid) return std::nullopt;
    SessionConfiguration config;
    config.capabilityId = configuration.capabilityId;
    config.selection = configuration;
    config.selection.selectedFiles = resolved.selectedFiles;
    config.runtimePath = resolved.runtimePath;
    config.familyConfig = resolved.family;
    config.resolvedPathsByRole = resolved.resolvedPaths;
    config.signature = resolved.signature;
    for (const auto &path : resolved.resolvedPaths) if (!path.toString().isEmpty()) config.resolvedModelPaths.append(path.toString());
    return config;
}
} // namespace LAStudio
