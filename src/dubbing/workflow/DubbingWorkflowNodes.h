#pragma once

#include "workflows/NodeRegistry.h"

namespace LAStudio {

class DubbingJobRunner;

// Registers the stable contract catalog for the system dubbing graph. Runtime
// implementations can be supplied by the Dubbing workflow adapter without
// changing graph type IDs or port IDs.
bool registerDubbingWorkflowNodes(NodeRegistry &registry);
bool registerDubbingWorkflowNodes(NodeRegistry &registry, DubbingJobRunner *runner);

} // namespace LAStudio
