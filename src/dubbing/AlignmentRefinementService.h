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

// A resolved, value-only alignment configuration.  Resolve this on the owner
// thread before dispatching refinement work; workers must not call QObject
// based model/runtime managers.
struct AlignmentRefinementConfiguration
{
    QString modelPath;
    QString modelId;
    QString runtimePath;
    QString runtimeId;
    QString runtimeVersion;
    QString runtimeKind;
    QString runtimeExecutable;

    bool isValid() const
    {
        if (modelPath.isEmpty()) return false;
        return runtimeKind == QStringLiteral("process")
            ? !runtimeExecutable.isEmpty() : !runtimePath.isEmpty();
    }
};

class AlignmentRefinementService final
{
public:
    static AlignmentRefinementConfiguration resolveConfiguration(ModelManager *models,
                                                                 RuntimeManager *runtimes);

    static AlignmentRefinementResult refine(const QString &audioPath,
                                             const QString &language,
                                             const QVariantList &segments,
                                             const AlignmentRefinementConfiguration &configuration,
                                             const QString &preset = QStringLiteral("balanced"),
                                             QAtomicInteger<bool> *cancel = nullptr);

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
