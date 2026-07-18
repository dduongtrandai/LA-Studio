#pragma once

#include "workflows/IWorkflowResolver.h"

namespace LAStudio {

class ModelManager;
class RuntimeManager;

struct AlignmentWorkflowPayload final : IWorkflowPayload {
    QVariantMap executionRequest;
    bool directProcessExecution = false;
};

class AlignmentWorkflowResolver final : public IWorkflowResolver {
public:
    AlignmentWorkflowResolver(const RuntimeManager *runtimes, const ModelManager *models);

    [[nodiscard]] WorkflowResolution resolve(const QVariantMap &request) const override;

private:
    const RuntimeManager *m_runtimes = nullptr;
    const ModelManager *m_models = nullptr;
};

} // namespace LAStudio
