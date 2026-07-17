#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include "core/StudioSelectionRepository.h"

namespace LAStudio {

// Shared lifecycle contract used by model sessions. Capability engines keep
// their own QML-facing enum so this type remains an internal boundary type.
enum class InferenceLifecycleState {
    Unconfigured,
    Unloaded,
    Loading,
    Ready,
    Processing,
    Unloading,
    Error
};

struct SessionConfiguration {
    QString capabilityId;
    StudioConfiguration selection;
    QString runtimePath;
    QVariantMap familyConfig;
    QStringList resolvedModelPaths;
    QVariantMap resolvedPathsByRole;
    QString signature;
};

} // namespace LAStudio
