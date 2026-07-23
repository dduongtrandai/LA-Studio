#pragma once

#include "TextNormalizationPolicy.h"
#include <QString>
#include <QVariantMap>

namespace LAStudio {

class TtsTextPreprocessor final {
public:
    static QString prepare(const QString &text,
                           const QVariantMap &studioConfig,
                           const QVariantMap &settings,
                           QString *profileId = nullptr);
};

} // namespace LAStudio
