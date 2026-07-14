#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LAStudio {

class TtsRequestValidator
{
public:
    static bool normalize(const QVariantList &schema,
                          const QVariantMap &studioConfig,
                          const QVariantMap &rawSettings,
                          QVariantMap &normalizedSettings,
                          QString &error);
};

} // namespace LAStudio
