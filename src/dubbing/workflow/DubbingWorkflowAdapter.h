#pragma once

#include "workflows/WorkflowExecutionAdapter.h"

namespace LAStudio {

class DubbingJobRunner;

// Bridges the reusable workflow node contract to the current Dubbing services.
// Other workflow families can provide their own adapter without changing node
// definitions or the graph runner.
class DubbingWorkflowAdapter final : public WorkflowExecutionAdapter
{
    Q_OBJECT
public:
    explicit DubbingWorkflowAdapter(DubbingJobRunner *runner, QObject *parent = nullptr);

    void start(const QString &nodeType, const QVariantMap &inputs,
               const QVariantMap &parameters) override;
    void cancel() override;
    void resume(const QVariantMap &decision) override;

private:
    DubbingJobRunner *m_runner = nullptr;
};

} // namespace LAStudio
