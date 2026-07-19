#include "controllers/dubbing/SourceSeparationConfigurationResolver.h"

#include "controllers/models/StudioConfigurationResolver.h"
#include "core/ModelManager.h"
#include "core/RuntimeManager.h"

#include <QFileInfo>

namespace LAStudio {

SourceSeparationConfigurationResult SourceSeparationConfigurationResolver::resolve(
    const QVariantMap &selection) const
{
    SourceSeparationConfigurationResult result;
    const QString familyId = selection.value(QStringLiteral("familyId")).toString();
    if (!familyId.isEmpty()) {
        StudioConfiguration configuration;
        configuration.capabilityId = QStringLiteral("voice-isolation");
        configuration.familyId = familyId;
        configuration.runtimeId = selection.value(QStringLiteral("runtimeId")).toString();
        configuration.runtimeVersion = selection.value(QStringLiteral("runtimeVersion")).toString();
        configuration.selectedFiles = selection.value(QStringLiteral("selectedFiles")).toMap();

        const ResolvedConfiguration resolved = StudioConfigurationResolver::resolve(configuration);
        if (!resolved.isValid || !QFileInfo(resolved.runtimePath).isFile()) {
            result.error = QStringLiteral("The selected voice isolation model or runtime is unavailable.");
            return result;
        }

        result.configuration.backendId = resolved.resolvedPaths.value(
            QStringLiteral("backend"), QStringLiteral("sherpa-onnx")).toString();
        result.configuration.pipelineProfile = resolved.resolvedPaths.value(QStringLiteral("pipelineProfile")).toString();
        result.configuration.runtimeId = configuration.runtimeId;
        result.configuration.runtimeVersion = configuration.runtimeVersion;
        result.configuration.runtimePath = resolved.runtimePath;
        result.configuration.familyId = configuration.familyId;
        result.configuration.configurationSignature = resolved.signature;

        for (const QVariant &requiredValue : resolved.family.value(QStringLiteral("requiredFiles")).toList()) {
            const QString role = requiredValue.toMap().value(QStringLiteral("role")).toString();
            const QString path = resolved.resolvedPaths.value(role).toString();
            if (role.isEmpty() || path.isEmpty() || !QFileInfo(path).isFile()) {
                result.error = QStringLiteral("A required voice isolation model file is unavailable: %1").arg(role);
                return result;
            }
            result.configuration.modelFilesByRole.insert(role, path);
        }
        result.available = true;
        return result;
    }

    QString runtimePath = qEnvironmentVariable("SHERPA_ONNX_RUNTIME");
    QString modelPath = qEnvironmentVariable("SHERPA_ONNX_UVR_MODEL");
    if ((runtimePath.isEmpty() || !QFileInfo(runtimePath).isFile()) && m_runtimes) {
        for (const QVariant &runtime : m_runtimes->allRuntimes()) {
            const QVariantMap value = runtime.toMap();
            const QString id = value.value(QStringLiteral("id")).toString();
            if (id != QStringLiteral("sherpa-onnx-win-x86_64-cpu")
                && id != QStringLiteral("sherpa-onnx-source-separation-win-x86_64-cpu")) continue;
            const QString path = m_runtimes->getRuntimePath(id);
            if (!path.isEmpty() && QFileInfo(path).isFile()) { runtimePath = path; break; }
        }
    }
    if ((modelPath.isEmpty() || !QFileInfo(modelPath).isFile()) && m_models) {
        for (const QString &id : {QStringLiteral("k2-fsa/sherpa-onnx-uvr-vocals-ft"),
                                  QStringLiteral("k2-fsa/sherpa-onnx-source-separation")}) {
            const QString path = m_models->filePath(id, QStringLiteral("UVR-MDX-NET-Voc_FT.onnx"));
            if (!path.isEmpty() && QFileInfo(path).isFile()) { modelPath = path; break; }
        }
    }

    if (runtimePath.isEmpty() || modelPath.isEmpty()
        || !QFileInfo(runtimePath).isFile() || !QFileInfo(modelPath).isFile()) {
        result.warning = QStringLiteral("Voice isolation runtime or model is unavailable; using normalized audio.");
        return result;
    }

    result.configuration.backendId = QStringLiteral("sherpa-onnx");
    result.configuration.pipelineProfile = QStringLiteral("uvr-2stems");
    result.configuration.runtimeId = QStringLiteral("sherpa-onnx-win-x86_64-cpu");
    result.configuration.runtimeVersion = QStringLiteral("v1.13.4");
    result.configuration.runtimePath = runtimePath;
    result.configuration.familyId = QStringLiteral("k2-fsa/sherpa-onnx-uvr-vocals-ft");
    result.configuration.configurationSignature = QStringLiteral("uvr-vocals-ft");
    result.configuration.modelFilesByRole.insert(QStringLiteral("model"), modelPath);
    result.available = true;
    return result;
}

} // namespace LAStudio
