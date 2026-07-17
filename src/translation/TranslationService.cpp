#include "TranslationService.h"

#include "IModelSession.h"
#include "core/ModelManager.h"
#include "core/RuntimeManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace LAStudio {
namespace {
void setError(QString *error, const QString &message) { if (error) *error = message; }

QString findModelPath(const QVariantMap &model)
{
    const QString modelDir = model.value(QStringLiteral("path")).toString();
    for (const QString &file : model.value(QStringLiteral("files")).toStringList()) {
        if (!file.endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) continue;
        const QString path = QDir(modelDir).absoluteFilePath(file);
        if (QFileInfo::exists(path)) return path;
    }
    if (QFileInfo(modelDir).isFile() && modelDir.endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) return modelDir;
    if (!modelDir.isEmpty()) {
        QDirIterator it(modelDir, {QStringLiteral("*.gguf")}, QDir::Files);
        if (it.hasNext()) return it.next();
    }
    return {};
}

QString backendForFamily(const QVariantMap &family)
{
    const QString id = family.value(QStringLiteral("id")).toString().toLower();
    if (id.contains(QStringLiteral("madlad"))) return QStringLiteral("madlad");
    if (id.contains(QStringLiteral("m2m"))) return QStringLiteral("m2m100");
    if (id.contains(QStringLiteral("hy-mt2")) || id.contains(QStringLiteral("hunyuan"))) return QStringLiteral("llama");
    const QVariantList architectures = family.value(QStringLiteral("architectures")).toList();
    for (const QVariant &value : architectures) {
        const QString architecture = value.toString().toLower();
        if (architecture.contains(QStringLiteral("madlad"))) return QStringLiteral("madlad");
        if (architecture.contains(QStringLiteral("m2m"))) return QStringLiteral("m2m100");
        if (architecture.contains(QStringLiteral("hunyuan"))) return QStringLiteral("llama");
    }
    return {};
}

bool fillRequest(const QString &modelPath, const QString &runtimePath, const QString &backend,
                 const QString &sourceLanguage, const QString &targetLanguage,
                 bool useGpu, TranslationRequest &request, QString *error)
{
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        setError(error, QStringLiteral("Translation model file is missing."));
        return false;
    }
    if (runtimePath.isEmpty() || !QFileInfo::exists(runtimePath)) {
        setError(error, QStringLiteral("Translation runtime executable or library is missing."));
        return false;
    }
    if (backend.isEmpty()) {
        setError(error, QStringLiteral("The selected model is not a supported translation backend."));
        return false;
    }
    request.modelPath = modelPath;
    request.runtimePath = runtimePath;
    request.backend = backend;
    request.sourceLanguage = sourceLanguage.trimmed();
    request.targetLanguage = targetLanguage.trimmed();
    request.useGpu = useGpu;
    return true;
}
}

TranslationService::TranslationService(ModelManager *models, RuntimeManager *runtimes)
    : m_models(models), m_runtimes(runtimes) {}

bool TranslationService::prepareFallback(const QString &sourceLanguage, const QString &targetLanguage,
                                         TranslationRequest &request, QString *error) const
{
    if (!m_models || !m_runtimes) {
        setError(error, QStringLiteral("Translation model and runtime managers are unavailable."));
        return false;
    }
    const QList<QPair<QString, QString>> candidates = {
        {QStringLiteral("tencent/Hy-MT2-1.8B-GGUF"), QStringLiteral("llama")},
        {QStringLiteral("cstr/m2m100-418m-GGUF"), QStringLiteral("m2m100")},
        {QStringLiteral("cstr/madlad400-3b-mt-GGUF"), QStringLiteral("madlad")}
    };
    for (const auto &candidate : candidates) {
        const QString modelPath = findModelPath(m_models->findModel(candidate.first));
        if (modelPath.isEmpty()) continue;
        for (const QVariant &runtimeValue : m_runtimes->allRuntimes()) {
            const QVariantMap runtime = runtimeValue.toMap();
            const QString engine = runtime.value(QStringLiteral("engineFamily")).toString();
            if ((candidate.second == QStringLiteral("llama") && engine != QStringLiteral("llama")) ||
                (candidate.second != QStringLiteral("llama") && engine != QStringLiteral("crispasr"))) continue;
            const QString runtimePath = runtime.value(QStringLiteral("kind")).toString() == QStringLiteral("process")
                ? runtime.value(QStringLiteral("executablePath")).toString()
                : runtime.value(QStringLiteral("libraryPath")).toString();
            if (!QFileInfo::exists(runtimePath)) continue;
            const QString id = runtime.value(QStringLiteral("id")).toString();
            return fillRequest(modelPath, runtimePath, candidate.second, sourceLanguage, targetLanguage,
                               id.contains(QStringLiteral("cuda"), Qt::CaseInsensitive) || id.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive),
                               request, error);
        }
    }
    setError(error, QStringLiteral("Install a Translation model and compatible runtime first."));
    return false;
}

bool TranslationService::prepareConfiguration(const SessionConfiguration &configuration,
                                              const QString &sourceLanguage, const QString &targetLanguage,
                                              TranslationRequest &request, QString *error)
{
    QString modelPath;
    for (const QString &path : configuration.resolvedModelPaths) {
        if (path.endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) { modelPath = path; break; }
    }
    const QString runtimeId = configuration.selection.runtimeId;
    return fillRequest(modelPath, configuration.runtimePath, backendForFamily(configuration.familyConfig),
                       sourceLanguage, targetLanguage,
                       runtimeId.contains(QStringLiteral("cuda"), Qt::CaseInsensitive) || runtimeId.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive),
                       request, error);
}

} // namespace LAStudio
