#pragma once

#include <QList>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

namespace LAStudio {

enum class WorkflowNodeKind { Input, Model, Runtime, Transform, Output };
enum class WorkflowNodeState { Unresolved, Missing, Blocked, Preparing, Ready, Running, Completed, Failed };

struct WorkflowResource {
    QString id;
    WorkflowNodeKind kind = WorkflowNodeKind::Model;
    QString title;
    QString resourceId;
    QString resolvedPath;
    WorkflowNodeState state = WorkflowNodeState::Unresolved;
    QString errorCode;
    QString statusText;
    bool required = true;

    [[nodiscard]] bool isReady() const noexcept { return !required || state == WorkflowNodeState::Ready; }
};

struct WorkflowPlanNode {
    QString id;
    WorkflowNodeKind kind = WorkflowNodeKind::Transform;
    QString title;
    QString resourceId;
    QString resolvedPath;
    WorkflowNodeState state = WorkflowNodeState::Unresolved;
    QString errorCode;
    QString statusText;
    QString providerResourceId;
    QString providerName;
    WorkflowNodeState providerState = WorkflowNodeState::Unresolved;
    QString providerStatusText;
    bool required = true;

    [[nodiscard]] bool isReady() const noexcept { return !required || state == WorkflowNodeState::Ready; }
    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct WorkflowEdge {
    QString sourceNodeId;
    QString targetNodeId;
};

struct WorkflowPlan {
    QString studioId;
    QString workflowId;
    QString signature;
    QList<WorkflowPlanNode> nodes;
    QList<WorkflowEdge> edges;
    QList<WorkflowResource> resources;

    [[nodiscard]] bool isReady() const noexcept;
    [[nodiscard]] int readyNodeCount() const noexcept;
    [[nodiscard]] QVariantList nodesAsVariantList() const;
    [[nodiscard]] const WorkflowPlanNode *firstBlockingNode() const noexcept;
};

struct IWorkflowPayload {
    virtual ~IWorkflowPayload() = default;
};

struct WorkflowResolution {
    WorkflowPlan plan;
    std::shared_ptr<const IWorkflowPayload> payload;
};

QString workflowNodeKindName(WorkflowNodeKind kind);
QString workflowNodeStateName(WorkflowNodeState state);

} // namespace LAStudio
