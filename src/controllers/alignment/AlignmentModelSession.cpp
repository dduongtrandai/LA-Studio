#include "controllers/alignment/AlignmentModelSession.h"

#include "controllers/alignment/AlignmentExecutionService.h"
#include "StudioConfigurationResolver.h"
#include "core/Logger.h"

#include <QFileInfo>

namespace LAStudio {

AlignmentModelSession::AlignmentModelSession(AlignmentExecutionService *service, QObject *parent)
    : IModelSession(parent)
    , m_service(service)
{
    if (m_service) {
        connect(m_service, &AlignmentExecutionService::stateChanged, this, &AlignmentModelSession::stateChanged);
        connect(m_service, &AlignmentExecutionService::completed, this, [this]() {
            clearError();
            emit stateChanged();
        });
        connect(m_service, &AlignmentExecutionService::failed, this, [this](const QString &, const QString &message) {
            setError(message);
        });
    }
}

ModelSessionState AlignmentModelSession::state() const
{
    if (m_activeSignature.isEmpty()) {
        return ModelSessionState::Unloaded;
    }
    if (m_service && m_service->processing()) {
        return ModelSessionState::Processing;
    }
    if (!m_error.isEmpty()) {
        return ModelSessionState::Error;
    }
    return ModelSessionState::Ready;
}

bool AlignmentModelSession::modelActive() const
{
    return !m_activeSignature.isEmpty();
}

bool AlignmentModelSession::canProcess() const
{
    return state() == ModelSessionState::Ready;
}

std::optional<SessionConfiguration> AlignmentModelSession::activeConfiguration() const
{
    if (m_activeSignature.isEmpty() || !m_loadedConfigs.contains(m_activeSignature)) {
        return std::nullopt;
    }
    return m_loadedConfigs.value(m_activeSignature);
}

std::optional<SessionConfiguration> AlignmentModelSession::pendingConfiguration() const
{
    return std::nullopt;
}

QList<SessionConfiguration> AlignmentModelSession::loadedConfigurations() const
{
    return m_loadedConfigs.values();
}

QString AlignmentModelSession::activeSignature() const
{
    return m_activeSignature;
}

void AlignmentModelSession::requestLoad(const QString &capabilityId, const StudioConfiguration &configuration)
{
    Q_UNUSED(capabilityId);
    auto resolved = resolveConfig(configuration);
    if (!resolved) {
        setError(QStringLiteral("Failed to resolve alignment configuration"));
        return;
    }

    clearError();
    m_loadedConfigs.insert(resolved->signature, *resolved);
    m_activeSignature = resolved->signature;
    Logger::info(QStringLiteral("AlignmentModelSession"),
                 QStringLiteral("Loaded alignment configuration: %1").arg(m_activeSignature));
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void AlignmentModelSession::requestUnload(const QString &capabilityId)
{
    Q_UNUSED(capabilityId);
    requestUnloadConfiguration(m_activeSignature);
}

void AlignmentModelSession::requestUnloadConfiguration(const QString &signature)
{
    if (signature.isEmpty()) {
        return;
    }
    m_loadedConfigs.remove(signature);
    if (m_activeSignature == signature) {
        m_activeSignature = m_loadedConfigs.isEmpty() ? QString() : m_loadedConfigs.constBegin().key();
    }
    clearError();
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void AlignmentModelSession::activateConfiguration(const QString &signature)
{
    if (signature.isEmpty() || !m_loadedConfigs.contains(signature)) {
        return;
    }
    m_activeSignature = signature;
    clearError();
    emit activeConfigurationChanged();
    emit activeSignatureChanged();
    emit stateChanged();
}

void AlignmentModelSession::requestReload(const QString &capabilityId)
{
    Q_UNUSED(capabilityId);
    auto active = activeConfiguration();
    if (active) {
        requestLoad(active->capabilityId, active->selection);
    }
}

bool AlignmentModelSession::usesRuntime(const QString &runtimeId, const QString &runtimeVersion) const
{
    for (const SessionConfiguration &config : loadedConfigurations()) {
        if (config.selection.runtimeId != runtimeId) {
            continue;
        }
        if (runtimeVersion.isEmpty() || config.selection.runtimeVersion == runtimeVersion) {
            return true;
        }
    }
    return false;
}

bool AlignmentModelSession::usesModelPath(const QString &modelPath) const
{
    auto cleanPath = [](const QString &path) {
        return QFileInfo(path).absoluteFilePath().replace(QStringLiteral("\\"), QStringLiteral("/"));
    };

    const QString target = cleanPath(modelPath);
    for (const SessionConfiguration &config : loadedConfigurations()) {
        for (const QString &path : config.resolvedModelPaths) {
            if (cleanPath(path).compare(target, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    return false;
}

std::optional<SessionConfiguration> AlignmentModelSession::resolveConfig(const StudioConfiguration &config) const
{
    auto resolved = StudioConfigurationResolver::resolve(config);
    if (!resolved.isValid) {
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

    for (const auto &value : resolved.resolvedPaths.values()) {
        const QString path = value.toString();
        if (!path.isEmpty()) {
            out.resolvedModelPaths.append(path);
        }
    }

    return out;
}

void AlignmentModelSession::setError(const QString &message)
{
    m_error = message;
    emit errorOccurred(message);
    emit stateChanged();
}

void AlignmentModelSession::clearError()
{
    m_error.clear();
}

} // namespace LAStudio
