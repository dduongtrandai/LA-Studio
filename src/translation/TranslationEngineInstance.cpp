#include "TranslationEngineInstance.h"

#include "TranslationWorker.h"
#include "core/Logger.h"

#include <QMetaObject>
#include <QFileInfo>

namespace LAStudio {

namespace {
QString translationBackendFor(const SessionConfiguration &configuration)
{
    const QString familyId = configuration.familyConfig.value(QStringLiteral("id")).toString()
        .isEmpty() ? configuration.selection.familyId
                   : configuration.familyConfig.value(QStringLiteral("id")).toString();
    const QString id = familyId.toLower();
    if (id.contains(QStringLiteral("madlad"))) return QStringLiteral("madlad");
    if (id.contains(QStringLiteral("m2m"))) return QStringLiteral("m2m100");
    if (id.contains(QStringLiteral("hy-mt2")) || id.contains(QStringLiteral("hunyuan"))) return QStringLiteral("llama");
    const QVariantList architectures = configuration.familyConfig.value(QStringLiteral("architectures")).toList();
    for (const QVariant &value : architectures) {
        const QString architecture = value.toString().toLower();
        if (architecture.contains(QStringLiteral("madlad"))) return QStringLiteral("madlad");
        if (architecture.contains(QStringLiteral("m2m"))) return QStringLiteral("m2m100");
        if (architecture.contains(QStringLiteral("hunyuan"))) return QStringLiteral("llama");
    }
    return configuration.familyConfig.value(QStringLiteral("backend")).toString().toLower();
}

QString translationModelPath(const SessionConfiguration &configuration)
{
    for (const QString &path : configuration.resolvedModelPaths) {
        if (path.endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) return path;
    }
    return configuration.resolvedModelPaths.value(0);
}
} // namespace

TranslationEngineInstance::TranslationEngineInstance(
    const SessionConfiguration &configuration,
    std::shared_ptr<TranslationBackendFactory> factory,
    QObject *parent)
    : QObject(parent), m_configuration(configuration), m_factory(std::move(factory))
{
    qRegisterMetaType<TranslationBackendConfiguration>();
    qRegisterMetaType<TranslationInferenceRequest>();
}

TranslationEngineInstance::~TranslationEngineInstance()
{
    unloadModelSync();
}

void TranslationEngineInstance::ensureWorker()
{
    if (m_thread) return;
    m_thread = new WorkerThreadHost(this);
    m_worker = new TranslationWorker(m_factory);
    m_thread->start(m_worker);
    connect(m_worker, &TranslationWorker::modelLoaded, this, &TranslationEngineInstance::onWorkerLoaded);
    connect(m_worker, &TranslationWorker::unloaded, this, &TranslationEngineInstance::onWorkerUnloaded);
    connect(m_worker, &TranslationWorker::progress, this, &TranslationEngineInstance::onWorkerProgress);
    connect(m_worker, &TranslationWorker::finished, this, &TranslationEngineInstance::onWorkerFinished);
    connect(m_worker, &TranslationWorker::errorOccurred, this, &TranslationEngineInstance::onWorkerError);
}

void TranslationEngineInstance::setState(State state)
{
    if (m_state == state) return;
    const bool loadedBefore = isModelLoaded();
    const bool processingBefore = isProcessing();
    m_state = state;
    if (loadedBefore != isModelLoaded()) emit modelLoadedChanged();
    if (processingBefore != isProcessing()) emit processingChanged();
    emit stateChanged();
}

void TranslationEngineInstance::loadModel()
{
    if (m_state == Loading || m_state == Processing) {
        Logger::warning(QStringLiteral("TranslationEngine"),
                        QStringLiteral("Load ignored signature=%1 state=%2").arg(signature()).arg(m_state));
        return;
    }
    ensureWorker();
    m_progress = 0;
    emit progressChanged();
    setState(Loading);
    const TranslationBackendConfiguration backendConfig{
        translationModelPath(m_configuration),
        m_configuration.runtimePath,
        translationBackendFor(m_configuration),
        m_configuration.selection.runtimeId.contains(QStringLiteral("cuda"), Qt::CaseInsensitive) ||
            m_configuration.selection.runtimeId.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive) ||
            m_configuration.selection.runtimeId.contains(QStringLiteral("hip"), Qt::CaseInsensitive) ||
            m_configuration.selection.runtimeId.contains(QStringLiteral("sycl"), Qt::CaseInsensitive) ||
            m_configuration.selection.runtimeId.contains(QStringLiteral("openvino"), Qt::CaseInsensitive),
        0};
    Logger::info(QStringLiteral("TranslationEngine"),
                 QStringLiteral("Loading model signature=%1 backend=%2 gpu=%3 model=%4 modelExists=%5 runtime=%6 runtimeExists=%7")
                     .arg(signature(), backendConfig.backendId)
                     .arg(backendConfig.useGpu ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(backendConfig.modelPath)
                     .arg(QFileInfo::exists(backendConfig.modelPath) ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(backendConfig.runtimePath)
                     .arg(QFileInfo::exists(backendConfig.runtimePath) ? QStringLiteral("true") : QStringLiteral("false")));
    QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                              Q_ARG(LAStudio::TranslationBackendConfiguration, backendConfig));
}

void TranslationEngineInstance::unloadModel()
{
    if (!m_thread) {
        setState(Unloaded);
        emit unloaded();
        return;
    }
    if (m_state == Unloaded) return;
    setState(Unloaded);
    QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::QueuedConnection);
}

void TranslationEngineInstance::unloadModelSync()
{
    if (!m_thread) return;
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::BlockingQueuedConnection);
    }
    stopThread();
    m_worker = nullptr;
}

void TranslationEngineInstance::stopThread()
{
    if (!m_thread) return;
    m_thread->stop();
    m_thread->deleteLater();
    m_thread = nullptr;
}

void TranslationEngineInstance::translate(const TranslationInferenceRequest &request)
{
    if (m_state != Ready || !m_worker) {
        Logger::error(QStringLiteral("TranslationEngine"),
                      QStringLiteral("Inference rejected signature=%1 state=%2 worker=%3 task=%4 segments=%5")
                          .arg(signature()).arg(m_state)
                          .arg(m_worker ? QStringLiteral("ready") : QStringLiteral("missing"))
                          .arg(request.task).arg(request.segments.size()));
        emit errorOccurred(QStringLiteral("Translation model is not ready."));
        return;
    }
    Logger::info(QStringLiteral("TranslationEngine"),
                 QStringLiteral("Inference queued signature=%1 task=%2 source=%3 target=%4 segments=%5 maxTokens=%6")
                     .arg(signature(), request.task, request.sourceLanguage, request.targetLanguage)
                     .arg(request.segments.size()).arg(request.maxTokens));
    m_progress = 0;
    emit progressChanged();
    m_activeCancellation = request.cancellation;
    setState(Processing);
    QMetaObject::invokeMethod(m_worker, "translate", Qt::QueuedConnection,
                              Q_ARG(LAStudio::TranslationInferenceRequest, request));
}

void TranslationEngineInstance::cancelProcessing()
{
    if (m_state != Processing || !m_worker) return;
    m_activeCancellation.cancel();
    QMetaObject::invokeMethod(m_worker, "cancelProcessing", Qt::QueuedConnection);
}

void TranslationEngineInstance::onWorkerLoaded(bool success, const QString &error)
{
    Logger::info(QStringLiteral("TranslationEngine"),
                 QStringLiteral("Model load finished signature=%1 success=%2 error=%3")
                     .arg(signature())
                     .arg(success ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(error));
    if (success) {
        setState(Ready);
        return;
    }
    setState(Error);
    emit errorOccurred(error);
}

void TranslationEngineInstance::onWorkerUnloaded()
{
    setState(Unloaded);
    emit unloaded();
}

void TranslationEngineInstance::onWorkerProgress(int percent)
{
    if (m_progress == percent) return;
    m_progress = percent;
    emit progressChanged();
}

void TranslationEngineInstance::onWorkerFinished(const QVariantList &patches)
{
    Logger::info(QStringLiteral("TranslationEngine"),
                 QStringLiteral("Inference finished signature=%1 patches=%2")
                     .arg(signature()).arg(patches.size()));
    setState(Ready);
    emit translationFinished(patches);
}

void TranslationEngineInstance::onWorkerError(const QString &error)
{
    Logger::error(QStringLiteral("TranslationEngine"),
                  QStringLiteral("Worker error signature=%1 state=%2 message=%3")
                      .arg(signature()).arg(m_state).arg(error));
    setState(Ready);
    emit errorOccurred(error);
}

} // namespace LAStudio
