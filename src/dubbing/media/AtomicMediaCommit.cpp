#include "dubbing/media/AtomicMediaCommit.h"

#include <QFile>
#include <QSaveFile>

namespace LAStudio {

bool AtomicMediaCommit::commit(const QString &stagingPath, const QString &destination,
                               QString *error)
{
    QFile source(stagingPath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Cannot open staged export: %1").arg(source.errorString());
        return false;
    }
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Cannot stage export commit: %1").arg(output.errorString());
        return false;
    }
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && !source.atEnd()) {
            if (error) *error = QStringLiteral("Cannot read staged export: %1").arg(source.errorString());
            return false;
        }
        if (output.write(chunk) != chunk.size()) {
            if (error) *error = QStringLiteral("Cannot write export: %1").arg(output.errorString());
            return false;
        }
    }
    if (!output.commit()) {
        if (error) *error = QStringLiteral("Cannot atomically commit export: %1").arg(output.errorString());
        return false;
    }
    return true;
}

} // namespace LAStudio
