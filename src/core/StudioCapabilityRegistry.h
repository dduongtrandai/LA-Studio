#pragma once
#include <QObject>
#include <QHash>
#include <QList>
#include <QVariantMap>
#include "StudioCapabilityDescriptor.h"

namespace LAStudio {
class IStudioAction;

class StudioCapabilityRegistry : public QObject {
    Q_OBJECT
public:
    static StudioCapabilityRegistry* instance();

    void registerCapability(const StudioCapabilityDescriptor &descriptor);
    StudioCapabilityDescriptor getCapability(const QString &capabilityId) const;
    QList<StudioCapabilityDescriptor> getAllCapabilities() const;
    bool hasCapability(const QString &capabilityId) const;
    IStudioAction* createAction(const QString &capabilityId, QObject *parent) const;
    bool familySupportsCapability(const QVariantMap &family, const QString &capabilityId) const;
    QString familyDomain(const QString &capabilityId) const;

private:
    explicit StudioCapabilityRegistry(QObject *parent = nullptr);
    ~StudioCapabilityRegistry() override = default;

    QHash<QString, StudioCapabilityDescriptor> m_descriptors;
};

} // namespace LAStudio
