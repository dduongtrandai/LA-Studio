#pragma once
#include <QObject>
#include <QString>
#include <QVariantMap>
#include "core/StudioSelectionRepository.h"

namespace LAStudio {

struct ResolvedConfiguration {
    QString runtimePath;
    QVariantMap family;
    QVariantMap resolvedPaths;
    QVariantMap selectedFiles;
    QString signature;
    bool isValid = false;
};

class StudioConfigurationResolver {
public:
    static ResolvedConfiguration resolve(const StudioConfiguration &config);
};

} // namespace LAStudio
