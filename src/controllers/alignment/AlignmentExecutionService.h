#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QAtomicInteger>
#include <QtQml/qqml.h>
#include "alignment/AlignmentWorkflowResolver.h"
#include "workflows/WorkflowSession.h"

namespace LAStudio {

class ModelManager;
class RuntimeManager;
class SttAudioDecoder;

class AlignmentExecutionService final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AlignmentExecutionService is managed by AppController")

    Q_PROPERTY(bool processing READ processing NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString output READ output NOTIFY resultChanged)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY resultChanged)
    Q_PROPERTY(double duration READ duration NOTIFY resultChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QVariantList diagnostics READ diagnostics NOTIFY resultChanged)
    Q_PROPERTY(double averageConfidence READ averageConfidence NOTIFY resultChanged)
    Q_PROPERTY(QVariantList karaokeLines READ karaokeLines NOTIFY resultChanged)
    Q_PROPERTY(QVariantList workflowNodes READ workflowNodes NOTIFY workflowChanged)
    Q_PROPERTY(bool workflowReady READ workflowReady NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowStatusText READ workflowStatusText NOTIFY workflowChanged)

public:
    explicit AlignmentExecutionService(RuntimeManager *runtimes,
                                       ModelManager *models,
                                       QObject *parent = nullptr);
    ~AlignmentExecutionService() override;

    bool processing() const { return m_pipelineProcessing || m_process.state() != QProcess::NotRunning; }
    QString statusText() const { return m_statusText; }
    QString errorCode() const { return m_errorCode; }
    QString errorMessage() const { return m_errorMessage; }
    QString output() const { return m_output; }
    QVariantList segments() const { return m_segments; }
    double duration() const { return m_duration; }
    QString stage() const { return m_stage; }
    int progress() const { return m_progress; }
    QVariantList diagnostics() const { return m_diagnostics; }
    double averageConfidence() const;
    QVariantList karaokeLines() const;
    QVariantList workflowNodes() const { return m_workflowSession.nodes(); }
    bool workflowReady() const { return m_workflowSession.ready(); }
    QString workflowStatusText() const { return m_workflowSession.statusText(); }

    Q_INVOKABLE bool align(const QString &runtimeId,
                           const QString &runtimeVersion,
                           const QString &modelId,
                           const QString &audioPath,
                           const QString &transcript,
                           const QString &language,
                           const QString &timestampUnit,
                           const QString &outputFormat,
                           bool normalizeTranscript,
                           int timeoutMs = 300000);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearResult();
    Q_INVOKABLE bool startPipeline(const QVariantMap &request);
    Q_INVOKABLE QVariantList installedAnchorModels() const;
    Q_INVOKABLE bool runStudioAlignment(const QVariantMap &request);
    Q_INVOKABLE bool prepareWorkflow(const QVariantMap &request);
    Q_INVOKABLE int segmentIndexAt(double seconds) const;
    Q_INVOKABLE int karaokeLineIndexAt(double seconds) const;

signals:
    void stateChanged();
    void resultChanged();
    void completed();
    void failed(const QString &code, const QString &message);
    void workflowChanged();

private:
    void setError(const QString &code, const QString &message);
    void handleFinished(int exitCode, QProcess::ExitStatus exitStatus);
    static QString localPath(const QString &pathOrUrl);
    void startPipelineWorker(const QVector<float> &samples, const QVariantMap &request);

    RuntimeManager *m_runtimes = nullptr;
    ModelManager *m_models = nullptr;
    QProcess m_process;
    QTimer m_timeout;
    QByteArray m_stdout;
    QByteArray m_stderr;
    QString m_statusText;
    QString m_errorCode;
    QString m_errorMessage;
    QString m_output;
    QVariantList m_segments;
    double m_duration = 0.0;
    bool m_cancelled = false;
    bool m_terminalProcessError = false;
    bool m_pipelineProcessing = false;
    QString m_stage;
    int m_progress = 0;
    QVariantList m_diagnostics;
    QAtomicInteger<bool> m_pipelineCancel = false;
    SttAudioDecoder *m_pipelineDecoder = nullptr;
    AlignmentWorkflowResolver m_workflowResolver;
    WorkflowSession m_workflowSession;
};

} // namespace LAStudio
