#pragma once

#include <QString>

namespace LAStudio {

class AtomicMediaCommit final
{
public:
    // Copies a completed staging artifact into a QSaveFile transaction.  The
    // destination remains untouched if the copy or commit fails.
    static bool commit(const QString &stagingPath, const QString &destination,
                       QString *error = nullptr);
};

} // namespace LAStudio
