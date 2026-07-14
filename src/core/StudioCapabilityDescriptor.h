#pragma once
#include <QString>

namespace LAStudio {

struct StudioCapabilityDescriptor {
    QString id;
    QString displayName;
    QString routeId;
    QString actionId;
    QString sharedEngineGroup;
    QString pageTitle;
    QString configurationTitle;
};

} // namespace LAStudio
