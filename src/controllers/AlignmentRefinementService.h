#pragma once

#include <QAtomicInteger>
#include <QString>
#include <QVariantList>

namespace LAStudio {

class ModelManager;
class RuntimeManager;

struct AlignmentRefinementResult
{
    QVariantList segments;
    bool attempted = false;
    bool changed = false;
    bool fromCache = false;
    QString status;
    QString diagnostic;
};

class AlignmentRefinementService final
{
public:
    // This method is deliberately dependency-light and safe to call from a
    // worker thread. It never mutates UI/session state and always returns the
    // original segments when alignment is unavailable or rejected.
    static AlignmentRefinementResult refine(const QString &audioPath,
                                             const QString &language,
                                             const QVariantList &segments,
                                             ModelManager *models,
                                             RuntimeManager *runtimes,
                                             const QString &preset = QStringLiteral("balanced"),
                                             QAtomicInteger<bool> *cancel = nullptr);
};

} // namespace LAStudio
