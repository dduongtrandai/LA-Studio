#include "LlamaTranslationBackend.h"
#include "core/Logger.h"

#include <QVariantMap>

namespace LAStudio {

bool LlamaTranslationBackend::loadModel(const TranslationBackendConfiguration &configuration,
                                        QString &error)
{
    Logger::info(QStringLiteral("LlamaTranslation"),
                 QStringLiteral("Loading gpu=%1 model=%2 runtime=%3")
                     .arg(configuration.useGpu ? QStringLiteral("true") : QStringLiteral("false"),
                          configuration.modelPath, configuration.runtimePath));
    m_configuration = configuration;
    return m_runtime.load(configuration.runtimePath, configuration.modelPath, &error,
                          configuration.useGpu);
}

void LlamaTranslationBackend::unloadModel()
{
    m_runtime.unload();
}

bool LlamaTranslationBackend::translate(const TranslationInferenceRequest &request,
                                        QVariantList &patches,
                                        TranslationProgressCallback progress,
                                        QString &error)
{
    patches.clear();
    QStringList texts;
    QStringList ids;
    for (const QVariant &value : request.segments) {
        const QVariantMap segment = value.toMap();
        const QString id = segment.value(QStringLiteral("id")).toString();
        const QString source = segment.value(QStringLiteral("sourceText")).toString().trimmed();
        if (id.isEmpty() || source.isEmpty()) {
            error = QStringLiteral("Every translation segment needs an id and source text.");
            return false;
        }
        ids.append(id);
        texts.append(source);
    }
    const QStringList translated = m_runtime.translateBatch(
        texts, request.sourceLanguage, request.targetLanguage, request.maxTokens,
        request.cancellation.sharedFlag(), &error,
        request.task,
        request.segments);
    if (translated.size() != ids.size()) {
        if (error.isEmpty()) error = QStringLiteral("Translation returned mismatched segment count.");
        return false;
    }
    for (int i = 0; i < translated.size(); ++i) {
        Logger::debug(QStringLiteral("LlamaTranslation"),
                      QStringLiteral("Segment translated id=%1 index=%2/%3 sourceChars=%4 targetChars=%5")
                          .arg(ids.at(i)).arg(i + 1).arg(ids.size())
                          .arg(texts.at(i).size()).arg(translated.at(i).size()));
        patches.append(QVariantMap{{QStringLiteral("id"), ids.at(i)},
                                   {QStringLiteral("targetText"), translated.at(i)},
                                   {QStringLiteral("state"), QStringLiteral("translated")}});
        if (progress) progress((i + 1) * 100 / qMax(1, translated.size()));
    }
    return true;
}

} // namespace LAStudio
