#include "VoiceIsolatorModelSession.h"

#include "StudioConfigurationResolver.h"
#include "VoiceIsolatorController.h"
#include "core/Logger.h"
#include "separation/SeparationTypes.h"

#include <QFileInfo>

namespace LAStudio {

VoiceIsolatorModelSession::VoiceIsolatorModelSession(VoiceIsolatorController *controller,
                                                     QObject *parent)
    : IModelSession(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &VoiceIsolatorController::stateChanged,
                this, &VoiceIsolatorModelSession::stateChanged);
    }
}

ModelSessionState VoiceIsolatorModelSession::state() const
{
    if (!m_error.isEmpty()) return ModelSessionState::Error;
    if (m_activeSignature.isEmpty()) return ModelSessionState::Unloaded;
    if (m_controller && m_controller->processing()) return ModelSessionState::Processing;
    return ModelSessionState::Ready;
}

bool VoiceIsolatorModelSession::modelActive() const
{
    return !m_activeSignature.isEmpty();
}

bool VoiceIsolatorModelSession::canProcess() const
{
    return state() == ModelSessionState::Ready;
}

std::optional<SessionConfiguration> VoiceIsolatorModelSession::activeConfiguration() const
{
    if (m_activeSignature.isEmpty() || !m_loadedConfigs.contains(m_activeSignature)) return std::nullopt;
    return m_loadedConfigs.value(m_activeSignature);
}

std::optional<SessionConfiguration> VoiceIsolatorModelSession::pendingConfiguration() const
{
    return std::nullopt;
}

QList<SessionConfiguration> VoiceIsolatorModelSession::loadedConfigurations() const
{
    return m_loadedConfigs.values();
}

QString VoiceIsolatorModelSession::activeSignature() const
{
    return m_activeSignature;
}

void VoiceIsolatorModelSession::requestLoad(const QString &capabilityId,
                                            const StudioConfiguration &configuration)
{
    Q_UNUSED(capabilityId);
    auto resolved = resolveConfig(configuration);
    if (!resolved) return;

    clearError();
    m_loadedConfigs.insert(resolved->signature, *resolved);
    m_activeSignature = resolved->signature;
    applyConfiguration(*resolved);

    Logger::info(QStringLiteral("VoiceIsolatorModelSession"),
                 QStringLiteral("Loaded voice isolation configuration: %1").arg(m_activeSignature));
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void VoiceIsolatorModelSession::requestUnload(const QString &capabilityId)
{
    Q_UNUSED(capabilityId);
    requestUnloadConfiguration(m_activeSignature);
}

void VoiceIsolatorModelSession::requestUnloadConfiguration(const QString &signature)
{
    if (signature.isEmpty()) return;

    m_loadedConfigs.remove(signature);
    if (m_activeSignature == signature) {
        m_activeSignature = m_loadedConfigs.isEmpty() ? QString() : m_loadedConfigs.constBegin().key();
        if (m_activeSignature.isEmpty()) {
            if (m_controller) m_controller->clearModelConfiguration();
        } else {
            applyConfiguration(m_loadedConfigs.value(m_activeSignature));
        }
    }
    clearError();
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void VoiceIsolatorModelSession::activateConfiguration(const QString &signature)
{
    if (signature.isEmpty() || !m_loadedConfigs.contains(signature)) return;
    m_activeSignature = signature;
    clearError();
    applyConfiguration(m_loadedConfigs.value(signature));
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void VoiceIsolatorModelSession::requestReload(const QString &capabilityId)
{
    Q_UNUSED(capabilityId);
    auto active = activeConfiguration();
    if (active) requestLoad(active->capabilityId, active->selection);
}

bool VoiceIsolatorModelSession::usesRuntime(const QString &runtimeId,
                                            const QString &runtimeVersion) const
{
    for (const SessionConfiguration &config : loadedConfigurations()) {
        if (config.selection.runtimeId != runtimeId) continue;
        if (runtimeVersion.isEmpty() || config.selection.runtimeVersion == runtimeVersion) return true;
    }
    return false;
}

bool VoiceIsolatorModelSession::usesModelPath(const QString &modelPath) const
{
    const QString target = QFileInfo(modelPath).absoluteFilePath();
    for (const SessionConfiguration &config : loadedConfigurations()) {
        for (const QString &path : config.resolvedModelPaths) {
            if (QFileInfo(path).absoluteFilePath().compare(target, Qt::CaseInsensitive) == 0) return true;
        }
    }
    return false;
}

std::optional<SessionConfiguration> VoiceIsolatorModelSession::resolveConfig(
    const StudioConfiguration &config)
{
    const ResolvedConfiguration resolved = StudioConfigurationResolver::resolve(config);
    if (!resolved.isValid) {
        setError(QStringLiteral("Failed to resolve voice isolation configuration."));
        return std::nullopt;
    }
    if (resolved.runtimePath.isEmpty() || !QFileInfo::exists(resolved.runtimePath)) {
        setError(QStringLiteral("The selected voice isolation runtime is not installed."));
        return std::nullopt;
    }

    SessionConfiguration out;
    out.capabilityId = config.capabilityId;
    out.selection = config;
    out.selection.selectedFiles = resolved.selectedFiles;
    out.runtimePath = resolved.runtimePath;
    out.familyConfig = resolved.family;
    out.resolvedPathsByRole = resolved.resolvedPaths;
    out.signature = resolved.signature;

    const QVariantList requiredFiles = resolved.family.value(QStringLiteral("requiredFiles")).toList();
    for (const QVariant &requiredValue : requiredFiles) {
        const QString role = requiredValue.toMap().value(QStringLiteral("role")).toString();
        const QString path = resolved.resolvedPaths.value(role).toString();
        if (role.isEmpty() || path.isEmpty() || !QFileInfo(path).isFile()) {
            setError(QStringLiteral("A required voice isolation model file is missing: %1").arg(role));
            return std::nullopt;
        }
        out.resolvedModelPaths.append(path);
    }
    return out;
}

void VoiceIsolatorModelSession::applyConfiguration(const SessionConfiguration &config)
{
    if (m_controller) {
        SeparationConfiguration sepConfig;
        
        sepConfig.backendId = config.familyConfig.value(QStringLiteral("backend")).toString();
        if (sepConfig.backendId.isEmpty()) {
            sepConfig.backendId = QStringLiteral("sherpa-onnx");
        }
        
        sepConfig.pipelineProfile = config.familyConfig.value(QStringLiteral("pipelineProfile")).toString();
        if (sepConfig.pipelineProfile.isEmpty()) {
            const QVariantList requiredFiles = config.familyConfig.value(QStringLiteral("requiredFiles")).toList();
            bool hasVocals = false;
            for (const QVariant &rf : requiredFiles) {
                if (rf.toMap().value(QStringLiteral("role")).toString() == QStringLiteral("vocals-model")) {
                    hasVocals = true;
                    break;
                }
            }
            sepConfig.pipelineProfile = hasVocals ? QStringLiteral("spleeter-2stems") : QStringLiteral("uvr-2stems");
        }
        
        sepConfig.runtimeId = config.selection.runtimeId;
        sepConfig.runtimeVersion = config.selection.runtimeVersion;
        sepConfig.runtimePath = config.runtimePath;
        sepConfig.familyId = config.selection.familyId;
        sepConfig.configurationSignature = config.signature;
        
        for (auto it = config.resolvedPathsByRole.begin(); it != config.resolvedPathsByRole.end(); ++it) {
            sepConfig.modelFilesByRole.insert(it.key(), it.value().toString());
        }
        
        m_controller->applySeparationConfiguration(sepConfig);
    }
}

void VoiceIsolatorModelSession::setError(const QString &message)
{
    m_error = message;
    emit errorOccurred(message);
    emit stateChanged();
}

void VoiceIsolatorModelSession::clearError()
{
    m_error.clear();
}

} // namespace LAStudio
