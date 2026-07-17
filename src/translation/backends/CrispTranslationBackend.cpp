#include "CrispTranslationBackend.h"

#include <QVariantMap>

namespace LAStudio {

bool CrispTranslationBackend::loadModel(const TranslationBackendConfiguration &configuration,
                                        QString &error)
{
    m_configuration = configuration;
    if (!m_runtime.load(configuration.runtimePath, configuration.modelPath, configuration.backendId,
                        configuration.threads, configuration.useGpu, &error)) {
        return false;
    }
    return true;
}

void CrispTranslationBackend::unloadModel()
{
    m_runtime.unload();
}

bool CrispTranslationBackend::translate(const TranslationInferenceRequest &request,
                                        QVariantList &patches,
                                        TranslationProgressCallback progress,
                                        QString &error)
{
    patches.clear();
    const int total = request.segments.size();
    int completed = 0;
    for (const QVariant &value : request.segments) {
        if (request.cancellation.isCancelled()) {
            error = QStringLiteral("Translation was cancelled.");
            return false;
        }
        const QVariantMap segment = value.toMap();
        const QString id = segment.value(QStringLiteral("id")).toString();
        const QString source = segment.value(QStringLiteral("sourceText")).toString().trimmed();
        if (id.isEmpty() || source.isEmpty()) {
            error = QStringLiteral("Every translation segment needs an id and source text.");
            return false;
        }
        const QString translated = m_runtime.translateLoaded(
            source, request.sourceLanguage, request.targetLanguage, request.maxTokens, &error);
        if (translated.isEmpty()) {
            if (error.isEmpty()) error = QStringLiteral("Translation returned no output.");
            return false;
        }
        patches.append(QVariantMap{{QStringLiteral("id"), id},
                                   {QStringLiteral("targetText"), translated},
                                   {QStringLiteral("state"), QStringLiteral("translated")}});
        ++completed;
        if (progress) progress(total > 0 ? (completed * 100) / total : 100);
    }
    return true;
}

} // namespace LAStudio
