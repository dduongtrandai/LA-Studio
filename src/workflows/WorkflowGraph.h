#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QJsonObject>
#include <QByteArray>

namespace LAStudio {

enum class WorkflowDataType { Any, Audio, Video, Text, TimedTranscript, SpeakerMap, AudioTrack, Media };

struct WorkflowPort {
    QString id;
    QString title;
    WorkflowDataType type = WorkflowDataType::Any;
    bool required = true;
    bool multiple = false;
    QVariant defaultValue;
};

struct WorkflowGraphNode {
    QString id;
    QString typeId;
    QString title;
    QVariantMap parameters;
    int typeVersion = 1;
    QVariantMap properties;
    bool disabled = false;
};

struct WorkflowGraphEdge {
    QString sourceNodeId;
    QString sourcePortId;
    QString targetNodeId;
    QString targetPortId;
    QString id;
};

using WorkflowLink = WorkflowGraphEdge;

struct WorkflowDiagnostic {
    QString severity = QStringLiteral("error");
    QString code;
    QString message;
    QString nodeId;
    QString portId;
    QString linkId;
    QString remediation;
};

struct WorkflowGraph {
    static constexpr int CurrentSchemaVersion = 1;

    QString id;
    QString title;
    int version = 1;
    QString kind = QStringLiteral("user");
    QString description;
    int schemaVersion = CurrentSchemaVersion;
    QList<WorkflowGraphNode> nodes;
    QList<WorkflowGraphEdge> edges;
    QVariantMap interfaceDefinition;
    QVariantMap policies;
    QVariantMap metadata;
    QVariantMap ui;

    const WorkflowGraphNode *node(const QString &nodeId) const;
    QStringList validate() const;
    QStringList topologicalOrder(QStringList *errors = nullptr) const;
    QJsonObject toJson(bool includeUi = true) const;
    static WorkflowGraph fromJson(const QJsonObject &json, QStringList *errors = nullptr);
    QByteArray canonicalJson() const;
};

QString workflowDataTypeName(WorkflowDataType type);
bool workflowDataTypesCompatible(WorkflowDataType output, WorkflowDataType input);

} // namespace LAStudio
