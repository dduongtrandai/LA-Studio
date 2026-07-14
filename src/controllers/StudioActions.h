#pragma once
#include "IStudioAction.h"
#include "IModelSession.h"

namespace LAStudio {

class CapabilityStudioAction final : public IStudioAction {
    Q_OBJECT
public:
    CapabilityStudioAction(const QString &capabilityId, IModelSession *session, QObject *parent = nullptr);
    ~CapabilityStudioAction() override = default;

    QString capabilityId() const override { return m_capabilityId; }
    QString activeConfigurationSignature() const override;
    StudioState state() const override;
    QString errorDetail() const override;

    void load(const StudioConfiguration &configuration) override;
    void unload() override;
    void reload() override;

private:
    QString m_capabilityId;
    IModelSession *m_session = nullptr;
    mutable QString m_errorDetail;
};

} // namespace LAStudio
