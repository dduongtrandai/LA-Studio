#pragma once

#include "NodeRegistry.h"
#include "WorkflowRun.h"
#include "WorkflowRunJournal.h"

#include <QObject>
#include <QPointer>
#include <QVariantMap>

namespace LAStudio {

class WorkflowGraphRunner final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString activeNodeId READ activeNodeId NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString runId READ runId NOTIFY stateChanged)
    Q_PROPERTY(QString nodeRunId READ nodeRunId NOTIFY stateChanged)
    Q_PROPERTY(bool waitingForInput READ waitingForInput NOTIFY stateChanged)

public:
    explicit WorkflowGraphRunner(NodeRegistry *registry, QObject *parent = nullptr);
    bool running() const { return m_running; }
    int progress() const { return m_progress; }
    QString activeNodeId() const { return m_activeNodeId; }
    QString error() const { return m_error; }
    QString runId() const { return m_runIdentity.runId; }
    QString nodeRunId() const { return m_runIdentity.nodeRunId; }
    bool waitingForInput() const { return m_waitingForInput; }

    QStringList validate(const WorkflowGraph &graph) const;
    bool run(const WorkflowGraph &graph, const QVariantMap &initialInputs = QVariantMap());
    void cancel();
    bool resume(const QVariantMap &decision);
    void setJournal(WorkflowRunJournal *journal) { m_journal = journal; }

signals:
    void stateChanged();
    void nodeStarted(const QString &nodeId);
    void nodeRunStarted(const QString &nodeId, const QString &nodeRunId);
    void reviewRequested(const QVariantMap &request);
    void nodeCompleted(const QString &nodeId, const QVariantMap &outputs);
    void completed(const QVariantMap &outputs);
    void failed(const QString &error);

private slots:
    void onExecutorProgress(int percent, const QString &status);
    void onExecutorCompleted(const QVariantMap &outputs);
    void onExecutorFailed(const QString &error);
    void onExecutorWaitingForInput(const QVariantMap &request);

private:
    void startNextNode();
    void fail(const QString &message);
    QVariantMap inputsForNode(const WorkflowGraphNode &node, QString *error) const;
    const WorkflowGraphEdge *incomingEdge(const QString &nodeId, const QString &portId) const;

    NodeRegistry *m_registry = nullptr;
    WorkflowGraph m_graph;
    WorkflowRunIdentity m_runIdentity;
    QStringList m_order;
    int m_orderIndex = 0;
    QVariantMap m_initialInputs;
    QVariantMap m_artifacts;
    QPointer<WorkflowNodeExecutor> m_executor;
    bool m_running = false;
    int m_progress = 0;
    QString m_activeNodeId;
    QString m_error;
    bool m_waitingForInput = false;
    WorkflowRunJournal *m_journal = nullptr;

    void journalEvent(const QString &eventType, const QJsonObject &payload = QJsonObject());
};

} // namespace LAStudio
