#include "NodeRegistry.h"

namespace LAStudio {

NodeRegistry::NodeRegistry(QObject *parent) : QObject(parent) {}

bool NodeRegistry::registerNode(const WorkflowNodeDefinition &definition)
{
    if (definition.typeId.trimmed().isEmpty() || definition.contractVersion < 1) return false;
    const QString key = definition.typeId + QLatin1Char('@') + QString::number(definition.contractVersion);
    if (m_definitions.contains(key)) return false;
    m_definitions.insert(key, definition);
    return true;
}

bool NodeRegistry::contains(const QString &typeId) const
{
    for (auto it = m_definitions.cbegin(); it != m_definitions.cend(); ++it) if (it.value().typeId == typeId) return true;
    return false;
}

bool NodeRegistry::contains(const QString &typeId, int contractVersion) const
{ return m_definitions.contains(typeId + QLatin1Char('@') + QString::number(contractVersion)); }

WorkflowNodeDefinition NodeRegistry::definition(const QString &typeId) const
{
    WorkflowNodeDefinition result;
    result.contractVersion = 0;
    for (auto it = m_definitions.cbegin(); it != m_definitions.cend(); ++it) {
        if (it.value().typeId == typeId && it.value().contractVersion > result.contractVersion)
            result = it.value();
    }
    return result;
}

WorkflowNodeDefinition NodeRegistry::definition(const QString &typeId, int contractVersion) const
{ return m_definitions.value(typeId + QLatin1Char('@') + QString::number(contractVersion)); }

QList<WorkflowNodeDefinition> NodeRegistry::definitions() const { return m_definitions.values(); }

WorkflowNodeExecutor *NodeRegistry::createExecutor(const QString &typeId, QObject *parent) const
{
    const auto item = definition(typeId);
    return item.createExecutor ? item.createExecutor(parent) : nullptr;
}

WorkflowNodeExecutor *NodeRegistry::createExecutor(const QString &typeId, int contractVersion, QObject *parent) const
{
    const auto item = definition(typeId, contractVersion);
    return item.createExecutor ? item.createExecutor(parent) : nullptr;
}

} // namespace LAStudio
