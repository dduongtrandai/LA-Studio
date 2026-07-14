#include "DubbingTranslationService.h"

#include "core/ModelManager.h"
#include "core/RuntimeManager.h"
#include "runtimes/CrispTranslationInterface.h"

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
        QDirIterator it(modelDir, QStringList{QStringLiteral("*.gguf")}, QDir::Files);
        if (it.hasNext()) return it.next();
    }
    return {};
}
}

DubbingTranslationService::DubbingTranslationService(ModelManager *models, RuntimeManager *runtimes)
    : m_models(models), m_runtimes(runtimes) {}

bool DubbingTranslationService::prepare(const QString &sourceLanguage, const QString &targetLanguage,
                                        DubbingTranslationRequest &request, QString *error) const
{
    if (!m_models || !m_runtimes) {
        setError(error, QStringLiteral("Translation model and runtime managers are unavailable."));
        return false;
    }
    const QList<QPair<QString, QString>> candidates = {
        {QStringLiteral("cstr/m2m100-418m-GGUF"), QStringLiteral("m2m100")},
        {QStringLiteral("cstr/madlad400-3b-mt-GGUF"), QStringLiteral("madlad")}
    };
    for (const auto &candidate : candidates) {
        const QString modelPath = findModelPath(m_models->findModel(candidate.first));
        if (modelPath.isEmpty()) continue;
        for (const QVariant &runtimeValue : m_runtimes->allRuntimes()) {
            const QVariantMap runtime = runtimeValue.toMap();
            if (runtime.value(QStringLiteral("engineFamily")).toString() != QStringLiteral("crispasr")) continue;
            const QString runtimePath = runtime.value(QStringLiteral("libraryPath")).toString();
            if (runtimePath.isEmpty() || !QFileInfo::exists(runtimePath)) continue;
            request.modelPath = modelPath;
            request.backend = candidate.second;
            request.runtimePath = runtimePath;
            request.sourceLanguage = sourceLanguage;
            request.targetLanguage = targetLanguage;
            request.useGpu = runtime.value(QStringLiteral("id")).toString().contains(QStringLiteral("cuda"), Qt::CaseInsensitive)
                || runtime.value(QStringLiteral("id")).toString().contains(QStringLiteral("vulkan"), Qt::CaseInsensitive);
            return true;
        }
    }
    setError(error, QStringLiteral("Install a CrispASR translation model and runtime first."));
    return false;
}

bool DubbingTranslationService::translate(const DubbingTranslationRequest &request,
                                          const QVariantList &segments, QVariantList &patches, QString *error)
{
    patches.clear();
    if (request.modelPath.isEmpty() || request.runtimePath.isEmpty()) {
        setError(error, QStringLiteral("Translation request has no resolved model or runtime."));
        return false;
    }
    CrispTranslationInterface translator;
    if (!translator.load(request.runtimePath)) {
        setError(error, translator.errorString());
        return false;
    }
    QStringList sourceTexts;
    QStringList segmentIds;
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
        const QString sourceText = segment.value(QStringLiteral("sourceText")).toString().trimmed();
        if (sourceText.isEmpty()) continue;
        const QString id = segment.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            setError(error, QStringLiteral("Cannot translate a segment without a stable id."));
            return false;
        }
        sourceTexts.append(sourceText);
        segmentIds.append(id);
    }
    if (sourceTexts.isEmpty()) return true;
    QString translationError;
    const QStringList translatedTexts = translator.translateBatch(request.modelPath, request.backend, sourceTexts,
                                                                   request.sourceLanguage, request.targetLanguage, 0,
                                                                   request.useGpu, 256, &translationError);
    if (translatedTexts.size() != sourceTexts.size()) {
        setError(error, translationError.isEmpty() ? QStringLiteral("Translation returned mismatched segment count.") : translationError);
        return false;
    }
    for (int i = 0; i < translatedTexts.size(); ++i) {
        patches.append(QVariantMap{{QStringLiteral("id"), segmentIds.at(i)},
                                    {QStringLiteral("targetText"), translatedTexts.at(i)},
                                    {QStringLiteral("state"), QStringLiteral("translated")}});
    }
    return true;
}

} // namespace LAStudio
