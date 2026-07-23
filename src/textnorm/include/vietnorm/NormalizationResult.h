#pragma once

#include <QString>
#include <QStringList>

namespace vietnorm {

struct NormalizationResult {
    QString text;
    QString profileId;
    QString dataVersion;
    QStringList warnings;
    bool changed = false;
};

} // namespace vietnorm
