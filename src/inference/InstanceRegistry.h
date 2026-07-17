#pragma once

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace LAStudio {

// Typed bookkeeping for capability engines. QObject ownership, signal
// forwarding and instance construction remain in the owning Engine.
template <typename Instance>
class InstanceRegistry final
{
public:
    Instance *value(const QString &signature) const { return m_instances.value(signature, nullptr); }
    bool contains(const QString &signature) const { return m_instances.contains(signature); }

    void insert(const QString &signature, Instance *instance) { m_instances.insert(signature, instance); }
    Instance *take(const QString &signature) { return m_instances.take(signature); }
    void clear() { m_instances.clear(); }

    QList<Instance *> values() const { return m_instances.values(); }
    QStringList signatures() const { return m_instances.keys(); }
    int size() const { return m_instances.size(); }

private:
    QHash<QString, Instance *> m_instances;
};

} // namespace LAStudio
