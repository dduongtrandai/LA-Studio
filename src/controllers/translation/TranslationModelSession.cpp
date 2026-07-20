#include "TranslationModelSession.h"
#include "StudioConfigurationResolver.h"
#include "translation/TranslationEngine.h"
#include <QFileInfo>

namespace LAStudio {
TranslationModelSession::TranslationModelSession(TranslationEngine *engine, QObject *parent)
    : IModelSession(parent), m_engine(engine)
{
    if (m_engine) {
        connect(m_engine, &TranslationEngine::stateChanged, this, &TranslationModelSession::onEngineStateChanged);
        connect(m_engine, &TranslationEngine::activeSignatureChanged, this, &TranslationModelSession::onEngineStateChanged);
        connect(m_engine, &TranslationEngine::errorOccurred, this, [this](const QString &error) {
            setError(error);
        });
    }
}
ModelSessionState TranslationModelSession::state() const
{
    if (!m_error.isEmpty()) return ModelSessionState::Error;
    if (!m_engine) return ModelSessionState::Unloaded;
    switch (m_engine->state()) {
    case TranslationEngine::Unloaded: return ModelSessionState::Unloaded;
    case TranslationEngine::Loading: return ModelSessionState::Loading;
    case TranslationEngine::Ready: return ModelSessionState::Ready;
    case TranslationEngine::Processing: return ModelSessionState::Processing;
    case TranslationEngine::Error: return ModelSessionState::Error;
    }
    return ModelSessionState::Unloaded;
}
bool TranslationModelSession::modelActive() const { return m_engine && m_engine->isModelLoaded(); }
bool TranslationModelSession::canProcess() const { return state() == ModelSessionState::Ready; }
std::optional<SessionConfiguration> TranslationModelSession::activeConfiguration() const
{
    if (m_engine) {
        if (auto *instance = m_engine->instance(m_engine->activeSignature())) return instance->configuration();
    }
    return m_loaded.contains(m_activeSignature) ? std::optional<SessionConfiguration>(m_loaded.value(m_activeSignature)) : std::nullopt;
}
std::optional<SessionConfiguration> TranslationModelSession::pendingConfiguration() const { return std::nullopt; }
QList<SessionConfiguration> TranslationModelSession::loadedConfigurations() const
{
    if (m_engine) return m_engine->loadedConfigurations();
    return m_loaded.values();
}
QString TranslationModelSession::activeSignature() const { return m_engine ? m_engine->activeSignature() : QString(); }
void TranslationModelSession::requestLoad(const QString &, const StudioConfiguration &configuration)
{
    auto resolved = resolveConfig(configuration);
    if (!resolved || !m_engine) {
        setError(QStringLiteral("Failed to resolve Translation configuration."));
        return;
    }
    clearError();
    m_loaded.insert(resolved->signature, *resolved);
    m_engine->loadInstance(*resolved, true);
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}
void TranslationModelSession::requestUnload(const QString &) { requestUnloadConfiguration(activeSignature()); }
void TranslationModelSession::requestUnloadConfiguration(const QString &signature) { if (signature.isEmpty()) return; m_loaded.remove(signature); if (m_engine) m_engine->unloadInstance(signature); clearError(); emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged(); }
void TranslationModelSession::activateConfiguration(const QString &signature) { if (!m_engine || !m_engine->instance(signature)) return; m_engine->activateInstance(signature); clearError(); emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged(); }
void TranslationModelSession::requestReload(const QString &) { auto active = activeConfiguration(); if (active) requestLoad(active->capabilityId, active->selection); }
bool TranslationModelSession::usesRuntime(const QString &id, const QString &version) const { for (const auto &config : loadedConfigurations()) if (config.selection.runtimeId == id && (version.isEmpty() || config.selection.runtimeVersion == version)) return true; return false; }
bool TranslationModelSession::usesModelPath(const QString &modelPath) const { const QString target = QFileInfo(modelPath).absoluteFilePath(); for (const auto &config : loadedConfigurations()) for (const QString &path : config.resolvedModelPaths) if (QFileInfo(path).absoluteFilePath().compare(target, Qt::CaseInsensitive) == 0) return true; return false; }
void TranslationModelSession::setProcessing(bool processing) { if (m_processing == processing) return; m_processing = processing; emit stateChanged(); }
void TranslationModelSession::setError(const QString &message) { m_error = message; emit errorOccurred(message); emit stateChanged(); }
void TranslationModelSession::clearError() { if (m_error.isEmpty()) return; m_error.clear(); emit stateChanged(); }
void TranslationModelSession::onEngineStateChanged() { emit stateChanged(); emit activeConfigurationChanged(); emit activeSignatureChanged(); }
std::optional<SessionConfiguration> TranslationModelSession::resolveConfig(const StudioConfiguration &configuration) const { auto resolved = StudioConfigurationResolver::resolve(configuration); if (!resolved.isValid) return std::nullopt; SessionConfiguration config; config.capabilityId = configuration.capabilityId; config.selection = configuration; config.selection.selectedFiles = resolved.selectedFiles; config.runtimePath = resolved.runtimePath; config.familyConfig = resolved.family; config.resolvedPathsByRole = resolved.resolvedPaths; config.signature = resolved.signature; for (const auto &path : resolved.resolvedPaths) if (!path.toString().isEmpty()) config.resolvedModelPaths.append(path.toString()); return config; }
} // namespace LAStudio
