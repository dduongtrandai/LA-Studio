#include "SourceSeparationService.h"
#include "SeparationWorker.h"
#include <QMetaObject>

namespace LAStudio {

SourceSeparationService::SourceSeparationService(QObject *parent)
    : QObject(parent)
    , m_factory(std::make_shared<SeparationBackendFactory>())
{
    init();
}

SourceSeparationService::SourceSeparationService(std::shared_ptr<SeparationBackendFactory> factory, QObject *parent)
    : QObject(parent)
    , m_factory(factory)
{
    init();
}

SourceSeparationService::~SourceSeparationService()
{
    cancel();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
    delete m_worker;
}

void SourceSeparationService::init()
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<SeparationConfiguration>();
        qRegisterMetaType<SeparationRequest>();
        qRegisterMetaType<SeparationStem>();
        qRegisterMetaType<SeparationResult>();
        qRegisterMetaType<SeparationErrorCode>();
        registered = true;
    }

    m_thread = new QThread(this);
    m_worker = new SeparationWorker(m_factory, nullptr);
    m_worker->moveToThread(m_thread);

    connect(m_worker, &SeparationWorker::progress, this, &SourceSeparationService::progress);
    connect(m_worker, &SeparationWorker::finished, this, &SourceSeparationService::onWorkerFinished);
    
    // Ensure worker is destroyed if thread exits
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
}

bool SourceSeparationService::isolate(const SeparationRequest &request, QString *error)
{
    if (m_processing) {
        if (error) {
            *error = QStringLiteral("Busy");
        }
        return false;
    }

    m_processing = true;
    m_cancelRequested.storeRelease(0);

    if (!m_thread->isRunning()) {
        m_thread->start();
    }

    QMetaObject::invokeMethod(m_worker, "process", Qt::QueuedConnection,
                              Q_ARG(SeparationRequest, request),
                              Q_ARG(QAtomicInt*, &m_cancelRequested));
    return true;
}

void SourceSeparationService::cancel()
{
    m_cancelRequested.storeRelease(1);
}

void SourceSeparationService::onWorkerFinished(const SeparationResult &result)
{
    m_processing = false;
    if (m_cancelRequested.loadAcquire()) {
        SeparationResult cancelledResult = result;
        cancelledResult.success = false;
        cancelledResult.errorCode = SeparationErrorCode::Cancelled;
        cancelledResult.error = QStringLiteral("Source separation cancelled.");
        emit finished(cancelledResult);
    } else {
        emit finished(result);
    }
}

} // namespace LAStudio
