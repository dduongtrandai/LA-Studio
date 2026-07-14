#include "WorkflowSession.h"

namespace LAStudio {

WorkflowSession::WorkflowSession(QObject *parent) : QObject(parent) {}

bool WorkflowSession::prepare(const IWorkflowResolver &resolver, const QVariantMap &request)
{
    m_resolution = resolver.resolve(request);
    emit changed();
    return ready();
}

void WorkflowSession::invalidate()
{
    if (m_resolution.plan.nodes.isEmpty() && m_resolution.plan.signature.isEmpty()) return;
    m_resolution = {};
    emit changed();
}

QString WorkflowSession::statusText() const
{
    const WorkflowPlan &current = plan();
    if (current.nodes.isEmpty()) return QStringLiteral("Not prepared");
    if (current.isReady())
        return QStringLiteral("Workflow ready (%1/%1)").arg(current.nodes.size());
    return QStringLiteral("Workflow needs setup (%1/%2)")
        .arg(current.readyNodeCount()).arg(current.nodes.size());
}

} // namespace LAStudio
