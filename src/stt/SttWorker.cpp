#include "SttWorker.h"
#include "backends/SttBackend.h"
#include "backends/SttBackendFactory.h"
#include "core/Logger.h"
#include <QElapsedTimer>

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
    Logger::info(QStringLiteral("SttWorker"),
                 QStringLiteral("Load model requested model=%1 gpu=%2 runtime=%3")
                     .arg(modelPath, useGpu ? QStringLiteral("true") : QStringLiteral("false"), runtimePath));
    unloadModel();

    QVariantMap config;
    config.insert(QStringLiteral("model"), modelPath);
    config.insert(QStringLiteral("runtimePath"), runtimePath);
    m_backend = SttBackendFactory::create(config);
    if (!m_backend) {
        Logger::error(QStringLiteral("SttWorker"), QStringLiteral("STT backend factory returned null"));
        emit modelLoaded(false, QStringLiteral("Unsupported STT backend configuration"));
        return;
    }

    QString error;
    if (!m_backend->loadModel(modelPath, useGpu, runtimePath, error)) {
        Logger::error(QStringLiteral("SttWorker"), QStringLiteral("STT model load failed: %1").arg(error));
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
    QElapsedTimer timer;
    timer.start();
    Logger::info(QStringLiteral("SttWorker"),
                 QStringLiteral("Transcription started samples=%1 language=%2 threads=%3 translate=%4 settings=%5")
                     .arg(samples.size()).arg(language).arg(threads)
                     .arg(translate ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(settings.keys().join(QLatin1Char(','))));
    if (!m_backend) {
        Logger::error(QStringLiteral("SttWorker"), QStringLiteral("Transcription rejected: no backend loaded"));
        emit errorOccurred(QStringLiteral("No model loaded"));
        return;
    }

    QString fullText;
    QVariantList segmentList;
    QString error;
    if (m_backend->transcribe(samples, language, threads, translate, settings, fullText, segmentList, error)) {
        Logger::info(QStringLiteral("SttWorker"),
                     QStringLiteral("Transcription finished segments=%1 textChars=%2 elapsedMs=%3")
                         .arg(segmentList.size()).arg(fullText.size()).arg(timer.elapsed()));
        emit finished(fullText, segmentList);
    } else {
        Logger::error(QStringLiteral("SttWorker"),
                      QStringLiteral("Transcription failed elapsedMs=%1 error=%2").arg(timer.elapsed()).arg(error));
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
