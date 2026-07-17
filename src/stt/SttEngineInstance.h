#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqml.h>
#include "inference/WorkerThreadHost.h"
#include <variant>

namespace LAStudio {

class SttWorker;

class SttEngineInstance : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SttEngineInstance is managed by SttEngine")

    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    enum State {
        Unloaded,
        Loading,
        Ready,
        Processing,
        Error
    };
    Q_ENUM(State)

    // State Machine States
    struct StateUnloaded {};
    struct StateLoading {
        bool cancelRequested = false;
        bool hasPendingLoad = false;
        QString pendingModelPath;
        bool pendingUseGpu = false;
        QString pendingRuntimePath;
    };
    struct StateReady {};
    struct StateProcessing {
        bool cancelRequested = false;
        bool stopRequested = false;
        bool hasPendingLoad = false;
        QString pendingModelPath;
        bool pendingUseGpu = false;
        QString pendingRuntimePath;
    };
    struct StateError { QString message; };
    
    using EngineState = std::variant<StateUnloaded, StateLoading, StateReady, StateProcessing, StateError>;

    // State Machine Events
    struct EventLoad { QString modelPath; bool useGpu; QString runtimePath; };
    struct EventUnload {};
    struct EventWorkerLoaded { bool success; QString error; };
    struct EventTranscribe { QVector<float> samples; QString language; int threads; bool translate; QVariantMap settings; };
    struct EventCancelProcessing {};
    struct EventWorkerFinished { QString text; QVariantList segments; };
    struct EventWorkerError { QString error; };

    using EngineEvent = std::variant<EventLoad, EventUnload, EventWorkerLoaded, EventTranscribe, EventCancelProcessing, EventWorkerFinished, EventWorkerError>;

    explicit SttEngineInstance(QObject *parent = nullptr);
    ~SttEngineInstance() override;

    State state() const;
    bool isModelLoaded() const { State s = state(); return s == Ready || s == Processing; }
    bool isProcessing() const { return state() == Processing; }
    int progress() const { return m_progress; }
    QString transcript() const { return m_transcript; }

    Q_INVOKABLE void loadModel(const QString &modelPath, bool useGpu = false, const QString &runtimePath = QString());
    Q_INVOKABLE void unloadModel();
    Q_INVOKABLE void unloadModelSync();
    Q_INVOKABLE void transcribeSamples(const QVector<float> &samples, const QString &language = "en", int threads = 0, bool translate = false, const QVariantMap &settings = QVariantMap());
    Q_INVOKABLE void cancelProcessing();

signals:
    void modelLoadedChanged();
    void processingChanged();
    void progressChanged();
    void transcriptChanged();
    void transcriptionFinished(const QString &text, const QVariantList &segments);
    void errorOccurred(const QString &error);
    void stateChanged();

private slots:
    void onWorkerModelLoaded(bool success, const QString &error);
    void onWorkerProgress(int percent);
    void onWorkerFinished(const QString &text, const QVariantList &segments);
    void onWorkerError(const QString &error);

private:
    void dispatch(const EngineEvent &event);
    void applyState(const EngineState &newState);

    SttWorker *m_worker = nullptr;
    WorkerThreadHost *m_thread = nullptr;
    EngineState m_state = StateUnloaded{};
    int m_progress = 0;
    QString m_transcript;
};

} // namespace LAStudio

