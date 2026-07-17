#pragma once

#include "backends/TranslationBackend.h"
#include "backends/TranslationBackendFactory.h"
#include "inference/InferenceCancellation.h"
#include "inference/InferenceTypes.h"
#include "inference/WorkerThreadHost.h"

#include <QObject>
#include <QVariantList>
#include <memory>
#include <variant>

namespace LAStudio {

class TranslationWorker;

class TranslationEngineInstance final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
public:
    enum State { Unloaded, Loading, Ready, Processing, Error };
    Q_ENUM(State)

    explicit TranslationEngineInstance(const SessionConfiguration &configuration,
                                       std::shared_ptr<TranslationBackendFactory> factory,
                                       QObject *parent = nullptr);
    ~TranslationEngineInstance() override;

    State state() const { return m_state; }
    bool isModelLoaded() const { return m_state == Ready || m_state == Processing; }
    bool isProcessing() const { return m_state == Processing; }
    int progress() const { return m_progress; }
    QString signature() const { return m_configuration.signature; }
    const SessionConfiguration &configuration() const { return m_configuration; }

    void loadModel();
    void unloadModel();
    void unloadModelSync();
    void translate(const TranslationInferenceRequest &request);
    void cancelProcessing();

signals:
    void modelLoadedChanged();
    void processingChanged();
    void progressChanged();
    void translationFinished(const QVariantList &patches);
    void errorOccurred(const QString &error);
    void stateChanged();
    void unloaded();

private slots:
    void onWorkerLoaded(bool success, const QString &error);
    void onWorkerUnloaded();
    void onWorkerProgress(int percent);
    void onWorkerFinished(const QVariantList &patches);
    void onWorkerError(const QString &error);

private:
    void setState(State state);
    void ensureWorker();
    void stopThread();

    SessionConfiguration m_configuration;
    std::shared_ptr<TranslationBackendFactory> m_factory;
    TranslationWorker *m_worker = nullptr;
    WorkerThreadHost *m_thread = nullptr;
    State m_state = Unloaded;
    int m_progress = 0;
    InferenceCancellationToken m_activeCancellation;
};

} // namespace LAStudio
