#include "WorkflowGraph.h"

#include <QHash>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>

namespace LAStudio {

QString workflowDataTypeName(WorkflowDataType type)
{
    switch (type) {
    case WorkflowDataType::Any: return QStringLiteral("any");
    case WorkflowDataType::Audio: return QStringLiteral("audio");
    case WorkflowDataType::Video: return QStringLiteral("video");
    case WorkflowDataType::Text: return QStringLiteral("text");
    case WorkflowDataType::TimedTranscript: return QStringLiteral("timed-transcript");
    case WorkflowDataType::SpeakerMap: return QStringLiteral("speaker-map");
    case WorkflowDataType::AudioTrack: return QStringLiteral("audio-track");
    case WorkflowDataType::Media: return QStringLiteral("media");
    }
    return QStringLiteral("any");
}

bool workflowDataTypesCompatible(WorkflowDataType output, WorkflowDataType input)
{
    return output == WorkflowDataType::Any || input == WorkflowDataType::Any || output == input;
}

namespace {
QJsonValue sortedJson(const QJsonValue &value)
{
    if (value.isArray()) {
        QJsonArray array;
        for (const auto &item : value.toArray()) array.append(sortedJson(item));
        return array;
    }
    if (!value.isObject()) return value;
    QMap<QString, QJsonValue> values;
    const auto object = value.toObject();
    for (const QString &key : object.keys()) values.insert(key, sortedJson(object.value(key)));
    QJsonObject result;
    for (auto it = values.cbegin(); it != values.cend(); ++it) result.insert(it.key(), it.value());
    return result;
}
}

const WorkflowGraphNode *WorkflowGraph::node(const QString &nodeId) const
{
    for (const auto &candidate : nodes) if (candidate.id == nodeId) return &candidate;
    return nullptr;
}

QStringList WorkflowGraph::validate() const
{
    QStringList errors;
    if (schemaVersion != CurrentSchemaVersion)
        errors.append(QStringLiteral("Unsupported graph schema version: %1").arg(schemaVersion));

    QSet<QString> ids;
    const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9._-]+$"));
    for (const auto &item : nodes) {
        if (item.id.trimmed().isEmpty()) errors.append(QStringLiteral("Graph node has an empty id."));
        if (ids.contains(item.id)) errors.append(QStringLiteral("Duplicate graph node id: %1").arg(item.id));
        ids.insert(item.id);
        if (!item.id.isEmpty() && !idPattern.match(item.id).hasMatch())
            errors.append(QStringLiteral("Invalid graph node id: %1").arg(item.id));
        if (item.typeId.trimmed().isEmpty()) errors.append(QStringLiteral("Graph node %1 has no type.").arg(item.id));
    }
    QSet<QString> linkIds;
    for (const auto &edge : edges) {
        if (!edge.id.isEmpty() && linkIds.contains(edge.id)) errors.append(QStringLiteral("Duplicate workflow link id: %1").arg(edge.id));
        if (!edge.id.isEmpty()) linkIds.insert(edge.id);
        if (!node(edge.sourceNodeId)) errors.append(QStringLiteral("Edge source node is missing: %1").arg(edge.sourceNodeId));
        if (!node(edge.targetNodeId)) errors.append(QStringLiteral("Edge target node is missing: %1").arg(edge.targetNodeId));
        if (edge.sourcePortId.trimmed().isEmpty() || edge.targetPortId.trimmed().isEmpty())
            errors.append(QStringLiteral("Graph edge must specify source and target ports."));
        if (edge.sourceNodeId == edge.targetNodeId)
            errors.append(QStringLiteral("Graph node cannot connect to itself: %1").arg(edge.sourceNodeId));
    }
    QStringList orderErrors;
    topologicalOrder(&orderErrors);
    errors.append(orderErrors);
    return errors;
}

QJsonObject WorkflowGraph::toJson(bool includeUi) const
{
    QJsonObject result{{QStringLiteral("schemaVersion"), schemaVersion}, {QStringLiteral("id"), id},
                       {QStringLiteral("version"), version}, {QStringLiteral("kind"), kind},
                       {QStringLiteral("title"), title}, {QStringLiteral("description"), description}};
    QJsonArray jsonNodes;
    for (const auto &node : nodes) {
        QJsonObject item{{QStringLiteral("id"), node.id}, {QStringLiteral("type"), node.typeId},
                         {QStringLiteral("typeVersion"), node.typeVersion}, {QStringLiteral("title"), node.title},
                         {QStringLiteral("properties"), QJsonObject::fromVariantMap(node.properties.isEmpty() ? node.parameters : node.properties)},
                         {QStringLiteral("disabled"), node.disabled}};
        jsonNodes.append(item);
    }
    result.insert(QStringLiteral("nodes"), jsonNodes);
    QJsonArray jsonLinks;
    for (const auto &link : edges) {
        QJsonObject from{{QStringLiteral("node"), link.sourceNodeId}, {QStringLiteral("port"), link.sourcePortId}};
        QJsonObject to{{QStringLiteral("node"), link.targetNodeId}, {QStringLiteral("port"), link.targetPortId}};
        jsonLinks.append(QJsonObject{{QStringLiteral("id"), link.id}, {QStringLiteral("from"), from}, {QStringLiteral("to"), to}});
    }
    result.insert(QStringLiteral("links"), jsonLinks);
    result.insert(QStringLiteral("interface"), QJsonObject::fromVariantMap(interfaceDefinition));
    result.insert(QStringLiteral("policies"), QJsonObject::fromVariantMap(policies));
    if (includeUi) {
        result.insert(QStringLiteral("metadata"), QJsonObject::fromVariantMap(metadata));
        result.insert(QStringLiteral("ui"), QJsonObject::fromVariantMap(ui));
    }
    return result;
}

WorkflowGraph WorkflowGraph::fromJson(const QJsonObject &json, QStringList *errors)
{
    WorkflowGraph graph;
    QStringList localErrors;
    graph.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(CurrentSchemaVersion);
    graph.id = json.value(QStringLiteral("id")).toString();
    graph.version = json.value(QStringLiteral("version")).toInt(1);
    graph.kind = json.value(QStringLiteral("kind")).toString(QStringLiteral("user"));
    graph.title = json.value(QStringLiteral("title")).toString();
    graph.description = json.value(QStringLiteral("description")).toString();
    graph.interfaceDefinition = json.value(QStringLiteral("interface")).toObject().toVariantMap();
    graph.policies = json.value(QStringLiteral("policies")).toObject().toVariantMap();
    graph.metadata = json.value(QStringLiteral("metadata")).toObject().toVariantMap();
    graph.ui = json.value(QStringLiteral("ui")).toObject().toVariantMap();
    for (const auto &value : json.value(QStringLiteral("nodes")).toArray()) {
        const auto item = value.toObject();
        if (item.isEmpty()) { localErrors.append(QStringLiteral("Workflow node must be an object.")); continue; }
        WorkflowGraphNode node;
        node.id = item.value(QStringLiteral("id")).toString();
        node.typeId = item.value(QStringLiteral("type")).toString();
        node.typeVersion = item.value(QStringLiteral("typeVersion")).toInt(1);
        node.title = item.value(QStringLiteral("title")).toString();
        node.properties = item.value(QStringLiteral("properties")).toObject().toVariantMap();
        node.parameters = node.properties;
        node.disabled = item.value(QStringLiteral("disabled")).toBool(false);
        graph.nodes.append(node);
    }
    const QJsonArray links = json.contains(QStringLiteral("links")) ? json.value(QStringLiteral("links")).toArray() : json.value(QStringLiteral("edges")).toArray();
    for (const auto &value : links) {
        const auto item = value.toObject();
        const auto from = item.value(QStringLiteral("from")).toObject();
        const auto to = item.value(QStringLiteral("to")).toObject();
        WorkflowGraphEdge link;
        link.id = item.value(QStringLiteral("id")).toString();
        link.sourceNodeId = from.value(QStringLiteral("node")).toString();
        link.sourcePortId = from.value(QStringLiteral("port")).toString();
        link.targetNodeId = to.value(QStringLiteral("node")).toString();
        link.targetPortId = to.value(QStringLiteral("port")).toString();
        graph.edges.append(link);
    }
    if (errors) *errors = localErrors;
    return graph;
}

QByteArray WorkflowGraph::canonicalJson() const
{
    return QJsonDocument(sortedJson(toJson(false)).toObject()).toJson(QJsonDocument::Compact);
}

QStringList WorkflowGraph::topologicalOrder(QStringList *errors) const
{
    QStringList localErrors;
    QHash<QString, int> indegree;
    QHash<QString, QStringList> outgoing;
    for (const auto &item : nodes) indegree.insert(item.id, 0);
    for (const auto &edge : edges) {
        if (!indegree.contains(edge.sourceNodeId) || !indegree.contains(edge.targetNodeId)) continue;
        outgoing[edge.sourceNodeId].append(edge.targetNodeId);
        ++indegree[edge.targetNodeId];
    }
    QStringList ready;
    for (const auto &item : nodes) if (indegree.value(item.id) == 0) ready.append(item.id);
    QStringList order;
    while (!ready.isEmpty()) {
        const QString current = ready.takeFirst();
        order.append(current);
        for (const QString &target : outgoing.value(current)) if (--indegree[target] == 0) ready.append(target);
    }
    if (order.size() != nodes.size()) localErrors.append(QStringLiteral("Workflow graph contains a cycle."));
    if (errors) *errors = localErrors;
    return order;
}

} // namespace LAStudio
