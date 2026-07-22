#include "RuntimeHostWorker.h"

#include "RuntimeHostAdapter.h"

namespace LAStudio {

RuntimeHostWorker::RuntimeHostWorker(QObject *parent) : QObject(parent) {}
RuntimeHostWorker::~RuntimeHostWorker() = default;

void RuntimeHostWorker::load(const QString &adapterId, const QCborMap &configuration)
{
    m_adapter = createRuntimeHostAdapter(adapterId);
    if (!m_adapter) {
        emit loadFinished(false, {}, QStringLiteral("Unsupported RuntimeHost adapter: %1").arg(adapterId));
        return;
    }
    m_adapter->setProgressCallback([this](int current, int total, const QString &stage,
                                           int chunkIndex, int chunkCount) {
        emit progress(current, total, stage, chunkIndex, chunkCount);
        return !m_cancelRequested.load(std::memory_order_acquire);
    });
    QCborValue schema;
    QString error;
    if (!m_adapter->load(configuration, &schema, &error)) {
        m_adapter.reset();
        emit loadFinished(false, {}, error);
        return;
    }
    m_cancelRequested = false;
    emit loadFinished(true, schema, {});
}

void RuntimeHostWorker::infer(const QCborMap &request, const QVector<float> &referenceSamples)
{
    if (!m_adapter) {
        emit inferFinished(false, {}, QStringLiteral("RuntimeHost model is not loaded."));
        return;
    }
    m_cancelRequested = false;
    RuntimeHostAdapter::Result result;
    QString error;
    const bool ok = m_adapter->execute(request, referenceSamples, &result, &error);
    if (m_cancelRequested.load(std::memory_order_acquire) && ok) {
        emit inferFinished(false, {}, QStringLiteral("Inference cancelled."));
        return;
    }
    emit inferFinished(ok, std::move(result), error);
}

void RuntimeHostWorker::cancel()
{
    requestCancel();
    emit cancelled();
}

void RuntimeHostWorker::requestCancel()
{
    m_cancelRequested.store(true, std::memory_order_release);
    if (m_adapter) m_adapter->cancel();
}

void RuntimeHostWorker::unload()
{
    if (m_adapter) m_adapter->unload();
    m_adapter.reset();
}

} // namespace LAStudio
