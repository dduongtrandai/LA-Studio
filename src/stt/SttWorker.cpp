#include "SttWorker.h"
#include "backends/SttBackend.h"
#include "backends/SttBackendFactory.h"
#include "core/Logger.h"

namespace LAStudio {

SttWorker::SttWorker(QObject *parent)
    : QObject(parent)
{
}

SttWorker::~SttWorker()
{
    unloadModel();
}

void SttWorker::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath)
{
    unloadModel();

    QVariantMap config;
    config.insert(QStringLiteral("model"), modelPath);
    config.insert(QStringLiteral("runtimePath"), runtimePath);
    m_backend = SttBackendFactory::create(config);
    if (!m_backend) {
        emit modelLoaded(false, QStringLiteral("Unsupported STT backend configuration"));
        return;
    }

    QString error;
    if (!m_backend->loadModel(modelPath, useGpu, runtimePath, error)) {
        m_backend.reset();
        emit modelLoaded(false, error);
        return;
    }

    Logger::info("SttWorker", "Model loaded: " + modelPath);
    emit modelLoaded(true, {});
}

void SttWorker::unloadModel()
{
    if (m_backend) {
        m_backend->unloadModel();
        m_backend.reset();
    }
}

void SttWorker::transcribe(const QVector<float> &samples, const QString &language, int threads, bool translate, const QVariantMap &settings)
{
    if (!m_backend) {
        emit errorOccurred(QStringLiteral("No model loaded"));
        return;
    }

    QString fullText;
    QVariantList segmentList;
    QString error;
    if (m_backend->transcribe(samples, language, threads, translate, settings, fullText, segmentList, error)) {
        emit finished(fullText, segmentList);
    } else {
        emit errorOccurred(error);
    }
}

void SttWorker::cancelProcessing()
{
    if (m_backend) {
        m_backend->cancelProcessing();
    }
}

} // namespace LAStudio
