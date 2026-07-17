#include "TranslationWorker.h"

namespace LAStudio {

TranslationWorker::TranslationWorker(std::shared_ptr<TranslationBackendFactory> factory,
                                     QObject *parent)
    : QObject(parent), m_factory(std::move(factory))
{
}

void TranslationWorker::loadModel(const TranslationBackendConfiguration &configuration)
{
    m_backend = m_factory ? m_factory->create(configuration.backendId) : nullptr;
    if (!m_backend) {
        emit modelLoaded(false, QStringLiteral("Unsupported translation backend: %1").arg(configuration.backendId));
        return;
    }
    QString error;
    if (!m_backend->loadModel(configuration, error)) {
        m_backend.reset();
        emit modelLoaded(false, error.isEmpty() ? QStringLiteral("Translation backend failed to load.") : error);
        return;
    }
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
    QVariantList patches;
    QString error;
    const bool ok = m_backend->translate(
        request, patches, [this](int percent) { emit progress(percent); }, error);
    if (!ok) {
        emit errorOccurred(error.isEmpty() ? QStringLiteral("Translation failed.") : error);
        return;
    }
    emit finished(patches);
}

void TranslationWorker::cancelProcessing()
{
    if (m_backend) m_backend->cancelProcessing();
}

} // namespace LAStudio
