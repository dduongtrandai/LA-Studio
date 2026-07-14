#pragma once

#include "WorkflowTypes.h"
#include <QVariantMap>

namespace LAStudio {

class IWorkflowResolver {
public:
    virtual ~IWorkflowResolver() = default;
    [[nodiscard]] virtual WorkflowResolution resolve(const QVariantMap &request) const = 0;
};

} // namespace LAStudio
