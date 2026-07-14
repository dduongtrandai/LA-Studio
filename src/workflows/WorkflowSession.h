#pragma once

#include "IWorkflowResolver.h"
#include <QObject>

namespace LAStudio {

class WorkflowSession final : public QObject {
    Q_OBJECT

public:
    explicit WorkflowSession(QObject *parent = nullptr);

    bool prepare(const IWorkflowResolver &resolver, const QVariantMap &request);
    void invalidate();

    [[nodiscard]] const WorkflowResolution &resolution() const noexcept { return m_resolution; }
    [[nodiscard]] const WorkflowPlan &plan() const noexcept { return m_resolution.plan; }
    [[nodiscard]] QVariantList nodes() const { return plan().nodesAsVariantList(); }
    [[nodiscard]] bool ready() const noexcept { return plan().isReady(); }
    [[nodiscard]] QString statusText() const;

signals:
    void changed();

private:
    WorkflowResolution m_resolution;
};

} // namespace LAStudio
