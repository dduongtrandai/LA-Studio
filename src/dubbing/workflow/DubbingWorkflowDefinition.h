#pragma once

#include "workflows/WorkflowGraph.h"

namespace LAStudio {

// Canonical graph definition used by Dubbing Studio and future user-facing
// workflow tooling. Runtime state is deliberately not stored here.
class DubbingWorkflowDefinition final
{
public:
    static constexpr const char *Id = "system.dubbing.default";
    static constexpr int Version = 3;

    static WorkflowGraph create();
};

} // namespace LAStudio
