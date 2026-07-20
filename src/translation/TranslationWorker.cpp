#include "TranslationWorker.h"
#include "core/Logger.h"

namespace LAStudio {

TranslationWorker::TranslationWorker(std::shared_ptr<TranslationBackendFactory> factory,
                                     QObject *parent)
    : QObject(parent), m_factory(std::move(factory))
{
}

void TranslationWorker::loadModel(const TranslationBackendConfiguration &configuration)
{
    Logger::info(QStringLiteral("TranslationWorker"),
                 QStringLiteral("Creating backend id=%1 gpu=%2 threads=%3 model=%4 runtime=%5")
                     .arg(configuration.backendId)
                     .arg(configuration.useGpu ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(configuration.threads)
                     .arg(configuration.modelPath, configuration.runtimePath));
    m_backend = m_factory ? m_factory->create(configuration.backendId) : nullptr;
    if (!m_backend) {
        emit modelLoaded(false, QStringLiteral("Unsupported translation backend: %1").arg(configuration.backendId));
        return;
    }
    QString error;
    if (!m_backend->loadModel(configuration, error)) {
        Logger::error(QStringLiteral("TranslationWorker"),
                      QStringLiteral("Backend load failed id=%1 error=%2")
                          .arg(configuration.backendId, error));
        m_backend.reset();
        emit modelLoaded(false, error.isEmpty() ? QStringLiteral("Translation backend failed to load.") : error);
        return;
    }
    Logger::info(QStringLiteral("TranslationWorker"),
                 QStringLiteral("Backend loaded id=%1").arg(configuration.backendId));
    emit modelLoaded(true, QString());
}

void TranslationWorker::unloadModel()
{
    if (m_backend) m_backend->unloadModel();
    m_backend.reset();
    emit unloaded();
}

void TranslationWorker::translate(const TranslationInferenceRequest &request)
{
    if (!m_backend || !m_backend->isLoaded()) {
        emit errorOccurred(QStringLiteral("Translation backend is not loaded."));
        return;
    }
    Logger::info(QStringLiteral("TranslationWorker"),
                 QStringLiteral("Backend inference started task=%1 segments=%2 source=%3 target=%4 maxTokens=%5")
                     .arg(request.task).arg(request.segments.size())
                     .arg(request.sourceLanguage, request.targetLanguage)
                     .arg(request.maxTokens));
    QVariantList patches;
    QString error;
    const bool ok = m_backend->translate(
        request, patches, [this](int percent) { emit progress(percent); }, error);
    if (!ok) {
        Logger::error(QStringLiteral("TranslationWorker"),
                      QStringLiteral("Backend inference failed task=%1 segments=%2 error=%3")
                          .arg(request.task).arg(request.segments.size()).arg(error));
        emit errorOccurred(error.isEmpty() ? QStringLiteral("Translation failed.") : error);
        return;
    }
    Logger::info(QStringLiteral("TranslationWorker"),
                 QStringLiteral("Backend inference completed task=%1 patches=%2")
                     .arg(request.task).arg(patches.size()));
    emit finished(patches);
}

void TranslationWorker::cancelProcessing()
{
    if (m_backend) m_backend->cancelProcessing();
}

} // namespace LAStudio
