#include "SttEngineInstance.h"
#include "SttWorker.h"
#include "audio/WavIO.h"

#include <QThreadPool>
#include <variant>

namespace LAStudio {

// Helper for std::visit
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

SttEngineInstance::SttEngineInstance(QObject *parent)
    : QObject(parent)
{
    m_worker = new SttWorker;
    m_thread = new WorkerThreadHost(this);
    m_thread->start(m_worker);
    connect(m_worker, &SttWorker::modelLoaded, this, &SttEngineInstance::onWorkerModelLoaded);
    connect(m_worker, &SttWorker::progress, this, &SttEngineInstance::onWorkerProgress);
    connect(m_worker, &SttWorker::finished, this, &SttEngineInstance::onWorkerFinished);
    connect(m_worker, &SttWorker::errorOccurred, this, &SttEngineInstance::onWorkerError);

}

SttEngineInstance::~SttEngineInstance()
{
    m_thread->stop();
}

SttEngineInstance::State SttEngineInstance::state() const
{
    return std::visit(overloaded{
        [](StateUnloaded) { return Unloaded; },
        [](StateLoading)  { return Loading; },
        [](StateReady)    { return Ready; },
        [](StateProcessing) { return Processing; },
        [](StateError)    { return Error; }
    }, m_state);
}

void SttEngineInstance::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath)
{
    dispatch(EventLoad{modelPath, useGpu, runtimePath});
}

void SttEngineInstance::unloadModel()
{
    dispatch(EventUnload{});
}

void SttEngineInstance::unloadModelSync()
{
    QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::BlockingQueuedConnection);
    applyState(StateUnloaded{});
}

void SttEngineInstance::transcribeSamples(const QVector<float> &samples, const QString &language, int threads, bool translate, const QVariantMap &settings)
{
    if (state() != State::Ready) {
        emit errorOccurred(QStringLiteral("No model loaded or engine not ready"));
        return;
    }
    dispatch(EventTranscribe{samples, language, threads, translate, settings});
}

void SttEngineInstance::cancelProcessing()
{
    dispatch(EventCancelProcessing{});
}

void SttEngineInstance::onWorkerModelLoaded(bool success, const QString &error)
{
    dispatch(EventWorkerLoaded{success, error});
}

void SttEngineInstance::onWorkerProgress(int percent)
{
    m_progress = percent;
    emit progressChanged();
}

void SttEngineInstance::onWorkerFinished(const QString &text, const QVariantList &segments)
{
    dispatch(EventWorkerFinished{text, segments});
}

void SttEngineInstance::onWorkerError(const QString &error)
{
    dispatch(EventWorkerError{error});
}

void SttEngineInstance::dispatch(const EngineEvent &event)
{
    std::visit(overloaded {
        // --- State: Unloaded ---
        [this](StateUnloaded&, const EventLoad& e) {
            applyState(StateLoading{});
            QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                                      Q_ARG(QString, e.modelPath), Q_ARG(bool, e.useGpu), Q_ARG(QString, e.runtimePath));
        },
        [this](StateUnloaded&, const auto&) {}, // Ignore other events in Unloaded

        // --- State: Loading ---
        [this](StateLoading& s, const EventLoad& e) {
            s.cancelRequested = true;
            s.hasPendingLoad = true;
            s.pendingModelPath = e.modelPath;
            s.pendingUseGpu = e.useGpu;
            s.pendingRuntimePath = e.runtimePath;
        },
        [this](StateLoading& s, const EventUnload&) {
            s.cancelRequested = true;
            s.hasPendingLoad = false;
            s.pendingModelPath.clear();
            s.pendingRuntimePath.clear();
        },
        [this](StateLoading& s, const EventWorkerLoaded& e) {
            bool cancelReq = s.cancelRequested;
            bool pending = s.hasPendingLoad;
            QString path = s.pendingModelPath;
            bool gpu = s.pendingUseGpu;
            QString rtPath = s.pendingRuntimePath;

            if (cancelReq) {
                QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::QueuedConnection);
                if (pending) {
                    applyState(StateLoading{});
                    QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                                              Q_ARG(QString, path), Q_ARG(bool, gpu), Q_ARG(QString, rtPath));
                } else {
                    applyState(StateUnloaded{});
                }
            } else {
                if (e.success) {
                    applyState(StateReady{});
                } else {
                    applyState(StateError{e.error});
                    emit errorOccurred(e.error);
                }
            }
        },
        [this](StateLoading&, const auto&) {},

        // --- State: Ready ---
        [this](StateReady&, const EventLoad& e) {
            applyState(StateLoading{});
            QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                                      Q_ARG(QString, e.modelPath), Q_ARG(bool, e.useGpu), Q_ARG(QString, e.runtimePath));
        },
        [this](StateReady&, const EventUnload&) {
            QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::QueuedConnection);
            applyState(StateUnloaded{});
        },
        [this](StateReady&, const EventTranscribe& e) {
            applyState(StateProcessing{});
            m_progress = 0;
            m_transcript.clear();
            emit progressChanged();
            emit transcriptChanged();
            QMetaObject::invokeMethod(m_worker, "transcribe", Qt::QueuedConnection,
                                      Q_ARG(QVector<float>, e.samples),
                                      Q_ARG(QString, e.language),
                                      Q_ARG(int, e.threads),
                                      Q_ARG(bool, e.translate),
                                      Q_ARG(QVariantMap, e.settings));
        },
        [this](StateReady&, const auto&) {},

        // --- State: Processing ---
        [this](StateProcessing& s, const EventLoad& e) {
            s.cancelRequested = true;
            s.hasPendingLoad = true;
            s.pendingModelPath = e.modelPath;
            s.pendingUseGpu = e.useGpu;
            s.pendingRuntimePath = e.runtimePath;
            QMetaObject::invokeMethod(m_worker, "cancelProcessing", Qt::DirectConnection);
        },
        [this](StateProcessing& s, const EventUnload&) {
            s.cancelRequested = true;
            s.hasPendingLoad = false;
            s.pendingModelPath.clear();
            s.pendingRuntimePath.clear();
            QMetaObject::invokeMethod(m_worker, "cancelProcessing", Qt::DirectConnection);
        },
        [this](StateProcessing& s, const EventCancelProcessing&) {
            s.stopRequested = true;
            s.hasPendingLoad = false;
            s.pendingModelPath.clear();
            s.pendingRuntimePath.clear();
            QMetaObject::invokeMethod(m_worker, "cancelProcessing", Qt::DirectConnection);
        },
        [this](StateProcessing& s, const EventWorkerFinished& e) {
            if (s.stopRequested) {
                applyState(StateReady{});
                return;
            }

            bool cancelReq = s.cancelRequested;
            bool pending = s.hasPendingLoad;
            QString path = s.pendingModelPath;
            bool gpu = s.pendingUseGpu;
            QString rtPath = s.pendingRuntimePath;

            if (cancelReq) {
                QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::QueuedConnection);
                if (pending) {
                    applyState(StateLoading{});
                    QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                                              Q_ARG(QString, path), Q_ARG(bool, gpu), Q_ARG(QString, rtPath));
                } else {
                    applyState(StateUnloaded{});
                }
            } else {
                m_progress = 100;
                m_transcript = e.text;
                emit progressChanged();
                emit transcriptChanged();
                applyState(StateReady{});
                emit transcriptionFinished(e.text, e.segments);
            }
        },
        [this](StateProcessing& s, const EventWorkerError& e) {
            if (s.stopRequested) {
                applyState(StateReady{});
                return;
            }

            bool cancelReq = s.cancelRequested;
            bool pending = s.hasPendingLoad;
            QString path = s.pendingModelPath;
            bool gpu = s.pendingUseGpu;
            QString rtPath = s.pendingRuntimePath;

            if (cancelReq) {
                QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::QueuedConnection);
                if (pending) {
                    applyState(StateLoading{});
                    QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                                              Q_ARG(QString, path), Q_ARG(bool, gpu), Q_ARG(QString, rtPath));
                } else {
                    applyState(StateUnloaded{});
                }
            } else {
                applyState(StateReady{});
                emit errorOccurred(e.error);
            }
        },
        [this](StateProcessing&, const auto&) {},

        // --- State: Error ---
        [this](StateError&, const EventLoad& e) {
            applyState(StateLoading{});
            QMetaObject::invokeMethod(m_worker, "loadModel", Qt::QueuedConnection,
                                      Q_ARG(QString, e.modelPath), Q_ARG(bool, e.useGpu), Q_ARG(QString, e.runtimePath));
        },
        [this](StateError&, const EventUnload&) {
            QMetaObject::invokeMethod(m_worker, "unloadModel", Qt::QueuedConnection);
            applyState(StateUnloaded{});
        },
        [this](StateError&, const auto&) {}

    }, m_state, event);
}

void SttEngineInstance::applyState(const EngineState &newState)
{
    State oldType = state();
    bool oldLoaded = isModelLoaded();
    bool oldProcessing = isProcessing();

    m_state = newState;
    
    State newType = state();
    if (oldType != newType) {
        emit stateChanged();
    }
    if (isModelLoaded() != oldLoaded) {
        emit modelLoadedChanged();
    }
    if (isProcessing() != oldProcessing) {
        emit processingChanged();
    }
}

} // namespace LAStudio
