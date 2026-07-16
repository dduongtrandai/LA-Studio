#include "TranslationModelSession.h"
#include "StudioConfigurationResolver.h"
#include <QFileInfo>

namespace LAStudio {
TranslationModelSession::TranslationModelSession(QObject *parent) : IModelSession(parent) {}
ModelSessionState TranslationModelSession::state() const { if (m_activeSignature.isEmpty()) return ModelSessionState::Unloaded; if (!m_error.isEmpty()) return ModelSessionState::Error; return m_processing ? ModelSessionState::Processing : ModelSessionState::Ready; }
bool TranslationModelSession::modelActive() const { return !m_activeSignature.isEmpty(); }
bool TranslationModelSession::canProcess() const { return state() == ModelSessionState::Ready; }
std::optional<SessionConfiguration> TranslationModelSession::activeConfiguration() const { return m_loaded.contains(m_activeSignature) ? std::optional<SessionConfiguration>(m_loaded.value(m_activeSignature)) : std::nullopt; }
std::optional<SessionConfiguration> TranslationModelSession::pendingConfiguration() const { return std::nullopt; }
QList<SessionConfiguration> TranslationModelSession::loadedConfigurations() const { return m_loaded.values(); }
QString TranslationModelSession::activeSignature() const { return m_activeSignature; }
void TranslationModelSession::requestLoad(const QString &, const StudioConfiguration &configuration) { auto resolved = resolveConfig(configuration); if (!resolved) { setError(QStringLiteral("Failed to resolve Translation configuration.")); return; } clearError(); m_loaded.insert(resolved->signature, *resolved); m_activeSignature = resolved->signature; emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged(); }
void TranslationModelSession::requestUnload(const QString &) { requestUnloadConfiguration(m_activeSignature); }
void TranslationModelSession::requestUnloadConfiguration(const QString &signature) { if (signature.isEmpty()) return; m_loaded.remove(signature); if (m_activeSignature == signature) m_activeSignature = m_loaded.isEmpty() ? QString() : m_loaded.constBegin().key(); clearError(); emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged(); }
void TranslationModelSession::activateConfiguration(const QString &signature) { if (!m_loaded.contains(signature)) return; m_activeSignature = signature; clearError(); emit activeConfigurationChanged(); emit activeSignatureChanged(); emit stateChanged(); }
void TranslationModelSession::requestReload(const QString &) { auto active = activeConfiguration(); if (active) requestLoad(active->capabilityId, active->selection); }
bool TranslationModelSession::usesRuntime(const QString &id, const QString &version) const { for (const auto &config : m_loaded) if (config.selection.runtimeId == id && (version.isEmpty() || config.selection.runtimeVersion == version)) return true; return false; }
bool TranslationModelSession::usesModelPath(const QString &modelPath) const { const QString target = QFileInfo(modelPath).absoluteFilePath(); for (const auto &config : m_loaded) for (const QString &path : config.resolvedModelPaths) if (QFileInfo(path).absoluteFilePath().compare(target, Qt::CaseInsensitive) == 0) return true; return false; }
void TranslationModelSession::setProcessing(bool processing) { if (m_processing == processing) return; m_processing = processing; emit stateChanged(); }
void TranslationModelSession::setError(const QString &message) { m_error = message; emit errorOccurred(message); emit stateChanged(); }
void TranslationModelSession::clearError() { if (m_error.isEmpty()) return; m_error.clear(); emit stateChanged(); }
std::optional<SessionConfiguration> TranslationModelSession::resolveConfig(const StudioConfiguration &configuration) const { auto resolved = StudioConfigurationResolver::resolve(configuration); if (!resolved.isValid) return std::nullopt; SessionConfiguration config; config.capabilityId = configuration.capabilityId; config.selection = configuration; config.runtimePath = resolved.runtimePath; config.familyConfig = resolved.family; config.resolvedPathsByRole = resolved.resolvedPaths; config.signature = resolved.signature; for (const auto &path : resolved.resolvedPaths) if (!path.toString().isEmpty()) config.resolvedModelPaths.append(path.toString()); return config; }
} // namespace LAStudio
