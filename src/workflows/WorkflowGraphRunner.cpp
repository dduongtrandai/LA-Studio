#include "WorkflowGraphRunner.h"

#include <QUuid>

namespace LAStudio {

WorkflowGraphRunner::WorkflowGraphRunner(NodeRegistry *registry, QObject *parent)
    : QObject(parent), m_registry(registry) {}

QStringList WorkflowGraphRunner::validate(const WorkflowGraph &graph) const
{
    QStringList errors = graph.validate();
    if (!m_registry) {
        errors.append(QStringLiteral("Workflow node registry is unavailable."));
        return errors;
    }
    for (const auto &node : graph.nodes) {
        if (!m_registry->contains(node.typeId, node.typeVersion)) {
            errors.append(QStringLiteral("Unknown workflow node type: %1@%2").arg(node.typeId).arg(node.typeVersion));
            continue;
        }
        const auto definition = m_registry->definition(node.typeId, node.typeVersion);
        QHash<QString, int> incoming;
        for (const auto &edge : graph.edges) {
            if (edge.sourceNodeId != node.id && edge.targetNodeId != node.id) continue;
            const bool sourceSide = edge.sourceNodeId == node.id;
            const QString portId = sourceSide ? edge.sourcePortId : edge.targetPortId;
            const auto ports = sourceSide ? definition.outputs : definition.inputs;
            bool found = false;
            for (const auto &port : ports) if (port.id == portId) { found = true; break; }
            if (!found) errors.append(QStringLiteral("Unknown %1 port %2 on node %3.")
                                      .arg(sourceSide ? QStringLiteral("output") : QStringLiteral("input"), portId, node.id));
            if (!sourceSide) ++incoming[portId];
        }
        for (const auto &port : definition.inputs) {
            const int count = incoming.value(port.id);
            if (count > 1 && !port.multiple)
                errors.append(QStringLiteral("Input port does not accept multiple links: %1.%2").arg(node.id, port.id));
        }
    }
    for (const auto &edge : graph.edges) {
        const auto *source = graph.node(edge.sourceNodeId);
        const auto *target = graph.node(edge.targetNodeId);
        if (!source || !target || !m_registry->contains(source->typeId, source->typeVersion) || !m_registry->contains(target->typeId, target->typeVersion)) continue;
        const auto sourceDefinition = m_registry->definition(source->typeId, source->typeVersion);
        const auto targetDefinition = m_registry->definition(target->typeId, target->typeVersion);
        WorkflowDataType sourceType = WorkflowDataType::Any, targetType = WorkflowDataType::Any;
        bool sourceFound = false, targetFound = false;
        for (const auto &port : sourceDefinition.outputs) if (port.id == edge.sourcePortId) { sourceType = port.type; sourceFound = true; }
        for (const auto &port : targetDefinition.inputs) if (port.id == edge.targetPortId) { targetType = port.type; targetFound = true; }
        if (sourceFound && targetFound && !workflowDataTypesCompatible(sourceType, targetType))
            errors.append(QStringLiteral("Incompatible ports: %1.%2 (%3) -> %4.%5 (%6).")
                          .arg(edge.sourceNodeId, edge.sourcePortId, workflowDataTypeName(sourceType),
                               edge.targetNodeId, edge.targetPortId, workflowDataTypeName(targetType)));
    }
    return errors;
}

bool WorkflowGraphRunner::run(const WorkflowGraph &graph, const QVariantMap &initialInputs)
{
    if (m_running) return false;
    const QStringList errors = validate(graph);
    if (!errors.isEmpty()) { fail(errors.join(QStringLiteral("\n"))); return false; }
    m_graph = graph;
    m_runIdentity = {};
    m_runIdentity.runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_runIdentity.workflowId = graph.id;
    m_runIdentity.workflowVersion = graph.version;
    journalEvent(QStringLiteral("run.queued"));
    m_order = graph.topologicalOrder();
    m_orderIndex = 0;
    m_initialInputs = initialInputs;
    m_artifacts.clear();
    m_error.clear();
    m_waitingForInput = false;
    m_progress = 0;
    m_activeNodeId.clear();
    m_runIdentity.nodeRunId.clear();
    m_running = true;
    emit stateChanged();
    journalEvent(QStringLiteral("run.started"));
    startNextNode();
    return true;
}

void WorkflowGraphRunner::cancel()
{
    if (!m_running) return;
    journalEvent(QStringLiteral("run.cancel_requested"));
    if (m_executor) m_executor->cancel();
    fail(QStringLiteral("Workflow run was canceled."));
}

bool WorkflowGraphRunner::resume(const QVariantMap &decision)
{
    if (!m_running || !m_waitingForInput || !m_executor) return false;
    m_waitingForInput = false;
    emit stateChanged();
    m_executor->resume(decision);
    return true;
}

const WorkflowGraphEdge *WorkflowGraphRunner::incomingEdge(const QString &nodeId, const QString &portId) const
{
    for (const auto &edge : m_graph.edges)
        if (edge.targetNodeId == nodeId && edge.targetPortId == portId) return &edge;
    return nullptr;
}

QVariantMap WorkflowGraphRunner::inputsForNode(const WorkflowGraphNode &node, QString *error) const
{
    QVariantMap result;
    const auto definition = m_registry->definition(node.typeId, node.typeVersion);
    for (const auto &port : definition.inputs) {
        const auto *edge = incomingEdge(node.id, port.id);
        const QString key = node.id + QLatin1Char('.') + port.id;
        if (!edge) {
            if (m_initialInputs.contains(key)) result.insert(port.id, m_initialInputs.value(key));
            else if (port.required) { if (error) *error = QStringLiteral("Required input is missing: %1").arg(key); return {}; }
            continue;
        }
        const QString artifactKey = edge->sourceNodeId + QLatin1Char('.') + edge->sourcePortId;
        if (!m_artifacts.contains(artifactKey)) { if (error) *error = QStringLiteral("Input artifact is not available: %1").arg(artifactKey); return {}; }
        result.insert(port.id, m_artifacts.value(artifactKey));
    }
    return result;
}

void WorkflowGraphRunner::startNextNode()
{
    if (!m_running) return;
    if (m_orderIndex >= m_order.size()) {
        journalEvent(QStringLiteral("run.completed"));
        m_running = false; m_activeNodeId.clear(); m_runIdentity.nodeRunId.clear(); m_progress = 100; emit stateChanged(); emit completed(m_artifacts); return;
    }
    const auto *node = m_graph.node(m_order.at(m_orderIndex));
    if (!node) { fail(QStringLiteral("Workflow node disappeared during execution.")); return; }
    QString inputError;
    const QVariantMap inputs = inputsForNode(*node, &inputError);
    if (!inputError.isEmpty()) { fail(inputError); return; }
    m_executor = m_registry->createExecutor(node->typeId, node->typeVersion, this);
    if (!m_executor) { fail(QStringLiteral("Cannot create executor for node: %1").arg(node->typeId)); return; }
    connect(m_executor, &WorkflowNodeExecutor::progress, this, &WorkflowGraphRunner::onExecutorProgress);
    connect(m_executor, &WorkflowNodeExecutor::completed, this, &WorkflowGraphRunner::onExecutorCompleted);
    connect(m_executor, &WorkflowNodeExecutor::failed, this, &WorkflowGraphRunner::onExecutorFailed);
    connect(m_executor, &WorkflowNodeExecutor::waitingForInput, this, &WorkflowGraphRunner::onExecutorWaitingForInput);
    m_activeNodeId = node->id;
    m_runIdentity.nodeId = node->id;
    m_runIdentity.nodeType = node->typeId;
    m_runIdentity.nodeContractVersion = node->typeVersion;
    m_runIdentity.nodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_runIdentity.attempt = 1;
    m_progress = m_order.isEmpty() ? 0 : (100 * m_orderIndex / m_order.size());
    emit stateChanged(); emit nodeStarted(node->id); emit nodeRunStarted(node->id, m_runIdentity.nodeRunId);
    journalEvent(QStringLiteral("node.started"), QJsonObject{{QStringLiteral("nodeId"), node->id},
                                                              {QStringLiteral("nodeType"), node->typeId}});
    m_executor->start(inputs, node->parameters);
}

void WorkflowGraphRunner::onExecutorProgress(int percent, const QString &status)
{
    Q_UNUSED(status);
    const int base = m_order.isEmpty() ? 0 : (100 * m_orderIndex / m_order.size());
    const int span = m_order.isEmpty() ? 100 : (100 / m_order.size());
    m_progress = qBound(0, base + qBound(0, percent, 100) * span / 100, 100);
    emit stateChanged();
}

void WorkflowGraphRunner::onExecutorCompleted(const QVariantMap &outputs)
{
    if (!m_running || !m_executor || sender() != m_executor.data()) return;
    const QString nodeId = m_activeNodeId;
    journalEvent(QStringLiteral("node.completed"), QJsonObject{{QStringLiteral("nodeId"), nodeId}});
    for (auto it = outputs.cbegin(); it != outputs.cend(); ++it) m_artifacts.insert(nodeId + QLatin1Char('.') + it.key(), it.value());
    emit nodeCompleted(nodeId, outputs);
    m_executor->deleteLater(); m_executor = nullptr; m_runIdentity.nodeRunId.clear(); ++m_orderIndex; startNextNode();
}

void WorkflowGraphRunner::onExecutorFailed(const QString &error)
{
    if (!m_running || !m_executor || sender() != m_executor.data()) return;
    fail(error);
}

void WorkflowGraphRunner::onExecutorWaitingForInput(const QVariantMap &request)
{
    if (!m_running || !m_executor || sender() != m_executor.data()) return;
    QVariantMap enriched = request;
    enriched.insert(QStringLiteral("nodeId"), m_activeNodeId);
    enriched.insert(QStringLiteral("nodeRunId"), m_runIdentity.nodeRunId);
    m_waitingForInput = true;
    emit stateChanged();
    journalEvent(QStringLiteral("node.waiting_for_input"), QJsonObject::fromVariantMap(enriched));
    emit reviewRequested(enriched);
}

void WorkflowGraphRunner::fail(const QString &message)
{
    if (m_executor) { m_executor->deleteLater(); m_executor = nullptr; }
    journalEvent(QStringLiteral("run.failed"), QJsonObject{{QStringLiteral("error"), message}});
    m_running = false; m_waitingForInput = false; m_error = message; m_activeNodeId.clear(); m_runIdentity.nodeRunId.clear(); emit stateChanged(); emit failed(message);
}

void WorkflowGraphRunner::journalEvent(const QString &eventType, const QJsonObject &payload)
{
    if (!m_journal || m_runIdentity.runId.isEmpty()) return;
    WorkflowRunEvent event;
    event.eventType = eventType;
    event.runId = m_runIdentity.runId;
    event.nodeRunId = m_runIdentity.nodeRunId;
    event.payload = payload;
    m_journal->append(event);
}

} // namespace LAStudio
