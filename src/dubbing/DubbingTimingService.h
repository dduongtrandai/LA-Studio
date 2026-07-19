#pragma once

#include <QAtomicInteger>
#include <QString>
#include <QVariantList>

namespace LAStudio {

class DubbingTimingService final
{
public:
    // Pure data boundary for timing fit.  The caller owns publication of the
    // returned list and the service never touches QObject/controller state.
    static QVariantList fitSegments(const QVariantList &segments,
                                    QAtomicInteger<bool> *cancel = nullptr,
                                    QString *error = nullptr);
};

} // namespace LAStudio
