#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LAStudio::CapabilitySettingsSchema {

// Combines catalog declarations with the schema discovered by the loaded
// runtime. Runtime fields (for example voice choices) are preserved while
// catalog fields provide stable labels, ranges, defaults, and descriptions.
QVariantList merge(const QVariantMap &familyConfig,
                   const QString &capabilityId,
                   const QVariantList &runtimeSchema);

} // namespace LAStudio::CapabilitySettingsSchema
