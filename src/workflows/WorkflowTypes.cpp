#include "WorkflowTypes.h"

#include <algorithm>

namespace LAStudio {

QString workflowNodeKindName(WorkflowNodeKind kind)
{
    switch (kind) {
    case WorkflowNodeKind::Input: return QStringLiteral("input");
    case WorkflowNodeKind::Model: return QStringLiteral("model");
    case WorkflowNodeKind::Runtime: return QStringLiteral("runtime");
    case WorkflowNodeKind::Output: return QStringLiteral("output");
    case WorkflowNodeKind::Transform: return QStringLiteral("transform");
    }
    return QStringLiteral("transform");
}

QString workflowNodeStateName(WorkflowNodeState state)
{
    switch (state) {
    case WorkflowNodeState::Unresolved: return QStringLiteral("unresolved");
    case WorkflowNodeState::Missing: return QStringLiteral("missing");
    case WorkflowNodeState::Blocked: return QStringLiteral("blocked");
    case WorkflowNodeState::Preparing: return QStringLiteral("preparing");
    case WorkflowNodeState::Ready: return QStringLiteral("ready");
    case WorkflowNodeState::Running: return QStringLiteral("running");
    case WorkflowNodeState::Completed: return QStringLiteral("completed");
    case WorkflowNodeState::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("unresolved");
}

QVariantMap WorkflowPlanNode::toVariantMap() const
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("type"), workflowNodeKindName(kind)},
            {QStringLiteral("title"), title},
            {QStringLiteral("resourceId"), resourceId},
            {QStringLiteral("resolvedPath"), resolvedPath},
            {QStringLiteral("state"), workflowNodeStateName(state)},
            {QStringLiteral("errorCode"), errorCode},
            {QStringLiteral("detail"), statusText},
            {QStringLiteral("providerResourceId"), providerResourceId},
            {QStringLiteral("providerName"), providerName},
            {QStringLiteral("providerState"), workflowNodeStateName(providerState)},
            {QStringLiteral("providerDetail"), providerStatusText},
            {QStringLiteral("required"), required}};
}

bool WorkflowPlan::isReady() const noexcept
{
    return !nodes.isEmpty()
        && std::all_of(nodes.cbegin(), nodes.cend(), [](const WorkflowPlanNode &node) {
        return node.isReady();
    }) && std::all_of(resources.cbegin(), resources.cend(), [](const WorkflowResource &resource) {
        return resource.isReady();
    });
}

int WorkflowPlan::readyNodeCount() const noexcept
{
    return int(std::count_if(nodes.cbegin(), nodes.cend(), [](const WorkflowPlanNode &node) {
        return node.isReady();
    }));
}

QVariantList WorkflowPlan::nodesAsVariantList() const
{
    QVariantList result;
    result.reserve(nodes.size());
    for (const WorkflowPlanNode &node : nodes) result.append(node.toVariantMap());
    return result;
}

const WorkflowPlanNode *WorkflowPlan::firstBlockingNode() const noexcept
{
    const auto it = std::find_if(nodes.cbegin(), nodes.cend(), [](const WorkflowPlanNode &node) {
        return !node.isReady();
    });
    return it == nodes.cend() ? nullptr : &(*it);
}

} // namespace LAStudio
