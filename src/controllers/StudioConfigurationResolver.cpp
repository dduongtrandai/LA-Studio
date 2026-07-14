#include "StudioConfigurationResolver.h"
#include "controllers/AppController.h"
#include "core/StudioCapabilityRegistry.h"
#include "core/RegistryManager.h"
#include "core/RuntimeManager.h"
#include "core/ModelManager.h"
#include "core/Settings.h"
#include <QVariantList>
#include <QDir>
#include <algorithm>

namespace LAStudio {

ResolvedConfiguration StudioConfigurationResolver::resolve(const StudioConfiguration &config)
{
    ResolvedConfiguration res;
    if (!config.isValid()) {
        return res;
    }

    AppController* app = AppController::instance();
    if (!app) {
        return res;
    }

    res.runtimePath = app->runtimes()->getRuntimePathForVersion(config.runtimeId, config.runtimeVersion);

    // Resolve the family configuration from the registry
    const QString domain = StudioCapabilityRegistry::instance()->familyDomain(config.capabilityId);
    const QVariantList allFamilies = (domain == QStringLiteral("stt"))
        ? app->registry()->sttFamilies()
        : app->registry()->ttsFamilies();

    for (const QVariant &item : allFamilies) {
        QVariantMap fam = item.toMap();
        if (fam.value(QStringLiteral("id")).toString() == config.familyId) {
            res.family = fam;
            break;
        }
    }

    if (res.family.isEmpty()) {
        return res; // Family not found in registry
    }

    // Resolve files
    QVariantList reqFiles = res.family.value(QStringLiteral("requiredFiles")).toList();
    for (const QVariant &reqVal : reqFiles) {
        QVariantMap req = reqVal.toMap();
        QString role = req.value(QStringLiteral("role")).toString();
        QString filename = config.selectedFiles.value(role).toString();
        if (filename.isEmpty()) filename = req.value(QStringLiteral("file")).toString();
        QString modelId = req.value(QStringLiteral("modelId")).toString();
        if (modelId.isEmpty()) modelId = res.family.value(QStringLiteral("modelId")).toString();

        QString path = app->models()->filePath(modelId, filename);
        if (path.isEmpty()) {
            path = QDir(app->models()->concreteModelDir(modelId)).absoluteFilePath(filename);
        }
        res.resolvedPaths.insert(role, path);
    }

    res.resolvedPaths.insert(QStringLiteral("familyId"), config.familyId);

    QVariantList runtimes = res.family.value(QStringLiteral("runtimes")).toList();
    for (const QVariant &rtVal : runtimes) {
        QVariantMap rt = rtVal.toMap();
        if (rt.value(QStringLiteral("id")).toString() == config.runtimeId) {
            if (!rt.value(QStringLiteral("disabledReason")).toString().trimmed().isEmpty()) {
                return {};
            }
            QString backend = rt.value(QStringLiteral("backend")).toString();
            if (backend.isEmpty()) backend = rt.value(QStringLiteral("engineFamily")).toString();
            res.resolvedPaths.insert(QStringLiteral("backend"), backend);
            res.resolvedPaths.insert(QStringLiteral("pipelineProfile"), rt.value(QStringLiteral("pipelineProfile")).toString());
            break;
        }
    }

    res.signature = config.signature(res.runtimePath, res.resolvedPaths);
    res.isValid = true;
    return res;
}

} // namespace LAStudio
