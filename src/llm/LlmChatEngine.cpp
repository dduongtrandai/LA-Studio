#include "LlmChatEngine.h"
#include "runtimes/LlamaTranslationInterface.h"
#include "runtimehost/RuntimeHostClient.h"
#include "runtimehost/RuntimeHostManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaType>
#include <QCborArray>

namespace LAStudio {

class LlmChatEngine::Worker final : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void load(const QString &runtimePath, const QString &modelPath, bool useGpu)
    {
        const QByteArray hostOverride = qgetenv("LASTUDIO_RUNTIME_HOST").trimmed().toLower();
        m_hosted = hostOverride != "0" && hostOverride != "off" && hostOverride != "false";
        if (m_hosted) {
            QString hostError;
            const bool gpu = useGpu || runtimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive);
            if (!RuntimeHostManager::instance().acquire(QStringLiteral("llama-chat"), gpu, &hostError)) {
                emit loaded(false, hostError);
                return;
            }
            m_gpuPermit = gpu;
            const QString hostPath = QDir(QCoreApplication::applicationDirPath())
                                         .absoluteFilePath(QStringLiteral("LAStudioRuntimeHost.exe"));
            if (QFileInfo(hostPath).isFile()
                && m_hostClient.start(hostPath, &hostError)) {
                const QCborMap config{
                    {QStringLiteral("adapter"), QStringLiteral("llama-chat")},
                    {QStringLiteral("runtimePath"), runtimePath},
                    {QStringLiteral("model"), modelPath},
                    {QStringLiteral("useGpu"), useGpu}
                };
                QCborValue schema;
                if (m_hostClient.load(config, &schema, &hostError)) {
                    emit loaded(true, {});
                    return;
                }
            }
            RuntimeHostManager::instance().release(QStringLiteral("llama-chat"), m_gpuPermit);
            m_gpuPermit = false;
            emit loaded(false, hostError.isEmpty()
                             ? QStringLiteral("Could not start llama RuntimeHost.") : hostError);
            return;
        }
        QString error;
        const bool ok = m_interface.load(runtimePath, modelPath, &error, useGpu);
        emit loaded(ok, error);
    }
    void unload()
    {
        if (m_hosted) {
            QString ignored;
            m_hostClient.shutdown(&ignored);
            RuntimeHostManager::instance().release(QStringLiteral("llama-chat"), m_gpuPermit);
            m_gpuPermit = false;
            m_hosted = false;
            emit unloaded();
            return;
        }
        m_interface.unload();
        emit unloaded();
    }
    void generate(const QList<QVariantMap> &messages, int contextTokens, int maxTokens,
                  float temperature, float topP, int topK, float repeatPenalty,
                  const QString &requestId)
    {
        auto cancelToken = std::make_shared<std::atomic_bool>(false);
        m_cancelToken = cancelToken;
        if (m_hosted) {
            m_hostClient.setProgressCallback([this, requestId](const QCborMap &progress) {
                emit tokenGenerated(requestId, progress.value(QStringLiteral("stage")).toString());
            });
            QVariantList messageList;
            for (const QVariantMap &message : messages) messageList.append(message);
            const QCborMap request{
                {QStringLiteral("messages"), QCborValue::fromVariant(messageList)},
                {QStringLiteral("contextTokens"), contextTokens},
                {QStringLiteral("maxTokens"), maxTokens},
                {QStringLiteral("temperature"), static_cast<double>(temperature)},
                {QStringLiteral("topP"), static_cast<double>(topP)},
                {QStringLiteral("topK"), topK},
                {QStringLiteral("repeatPenalty"), static_cast<double>(repeatPenalty)}
            };
            QCborMap result;
            QString error;
            const bool ok = m_hostClient.execute(request, {}, &result, nullptr, nullptr, &error);
            m_hostClient.setProgressCallback({});
            m_cancelToken.reset();
            if (cancelToken->load(std::memory_order_relaxed)) {
                emit cancelled(requestId, result.value(QStringLiteral("fullText")).toString());
            } else if (ok) {
                emit finished(requestId, result.value(QStringLiteral("fullText")).toString());
            } else {
                emit failed(requestId, error);
            }
            return;
        }
        QString text;
        QString error;
        const bool ok = m_interface.generateChat(
            messages, contextTokens, maxTokens, temperature, topP, topK, repeatPenalty,
            cancelToken,
            [this, requestId](const QString &token) { emit tokenGenerated(requestId, token); },
            &text, &error);
        m_cancelToken.reset();
        if (cancelToken->load(std::memory_order_relaxed)) {
            emit cancelled(requestId, text);
        } else if (ok) {
            emit finished(requestId, text);
        } else {
            emit failed(requestId, error);
        }
    }
    void cancel()
    {
        if (m_hosted) {
            if (m_cancelToken) m_cancelToken->store(true, std::memory_order_relaxed);
            m_hostClient.cancelCurrent();
            return;
        }
        if (m_cancelToken) m_cancelToken->store(true, std::memory_order_relaxed);
        m_interface.cancel();
    }

signals:
    void loaded(bool ok, const QString &error);
    void unloaded();
    void tokenGenerated(const QString &requestId, const QString &token);
    void finished(const QString &requestId, const QString &text);
    void cancelled(const QString &requestId, const QString &text);
    void failed(const QString &requestId, const QString &message);

private:
    LlamaTranslationInterface m_interface;
    RuntimeHostClient m_hostClient;
    bool m_hosted = false;
    bool m_gpuPermit = false;
    std::shared_ptr<std::atomic_bool> m_cancelToken;
};

LlmChatEngine::LlmChatEngine(QObject *parent)
    : QObject(parent), m_worker(new Worker)
{
    qRegisterMetaType<QList<QVariantMap>>("QList<QVariantMap>");
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &Worker::loaded, this, &LlmChatEngine::onLoaded, Qt::QueuedConnection);
    connect(m_worker, &Worker::unloaded, this, &LlmChatEngine::onUnloaded, Qt::QueuedConnection);
    connect(m_worker, &Worker::tokenGenerated, this, &LlmChatEngine::onToken, Qt::QueuedConnection);
    connect(m_worker, &Worker::finished, this, &LlmChatEngine::onFinished, Qt::QueuedConnection);
    connect(m_worker, &Worker::cancelled, this, &LlmChatEngine::onCancelled, Qt::QueuedConnection);
    connect(m_worker, &Worker::failed, this, &LlmChatEngine::onError, Qt::QueuedConnection);
    m_thread.start();
}

LlmChatEngine::~LlmChatEngine()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "cancel", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(m_worker, "unload", Qt::BlockingQueuedConnection);
    }
    m_thread.quit();
    m_thread.wait();
}

void LlmChatEngine::load(const QString &runtimePath, const QString &modelPath, bool useGpu)
{
    m_state = Loading;
    emit stateChanged();
    QMetaObject::invokeMethod(m_worker, "load", Qt::QueuedConnection,
                              Q_ARG(QString, runtimePath), Q_ARG(QString, modelPath), Q_ARG(bool, useGpu));
}

void LlmChatEngine::unload()
{
    m_state = Unloaded;
    m_modelLoaded = false;
    m_processing = false;
    emit modelLoadedChanged();
    emit processingChanged();
    emit stateChanged();
    QMetaObject::invokeMethod(m_worker, "unload", Qt::QueuedConnection);
}

void LlmChatEngine::generate(const QList<QVariantMap> &messages, int contextTokens, int maxTokens,
                             float temperature, float topP, int topK, float repeatPenalty,
                             const QString &requestId)
{
    if (!m_modelLoaded || m_processing) return;
    m_requestId = requestId;
    m_processing = true;
    m_state = Processing;
    emit processingChanged();
    emit stateChanged();
    QMetaObject::invokeMethod(m_worker, "generate", Qt::QueuedConnection,
                              Q_ARG(QList<QVariantMap>, messages), Q_ARG(int, contextTokens),
                              Q_ARG(int, maxTokens), Q_ARG(float, temperature), Q_ARG(float, topP),
                              Q_ARG(int, topK), Q_ARG(float, repeatPenalty), Q_ARG(QString, requestId));
}

void LlmChatEngine::cancel()
{
    if (!m_processing) return;
    QMetaObject::invokeMethod(m_worker, "cancel", Qt::QueuedConnection);
}

void LlmChatEngine::onLoaded(bool ok, const QString &error)
{
    m_modelLoaded = ok;
    m_state = ok ? Ready : Error;
    emit modelLoadedChanged();
    emit stateChanged();
    if (!ok) emit errorOccurred(error);
}
void LlmChatEngine::onUnloaded() {}
void LlmChatEngine::onToken(const QString &requestId, const QString &token) { emit tokenGenerated(requestId, token); }
void LlmChatEngine::onFinished(const QString &requestId, const QString &text)
{
    m_processing = false; m_state = Ready; emit processingChanged(); emit stateChanged();
    emit generationFinished(requestId, text);
}
void LlmChatEngine::onCancelled(const QString &requestId, const QString &text)
{
    m_processing = false; m_state = Ready; emit processingChanged(); emit stateChanged();
    emit generationCancelled(requestId, text);
}
void LlmChatEngine::onError(const QString &, const QString &message)
{
    m_processing = false; m_state = Error; emit processingChanged(); emit stateChanged();
    emit errorOccurred(message);
}

} // namespace LAStudio

#include "LlmChatEngine.moc"
