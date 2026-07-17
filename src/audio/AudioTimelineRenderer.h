#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace LAStudio {

struct AudioRenderResult {
    bool usedFallback = false;
};

struct AudioTimelinePlacement {
    qint64 startMs = 0;
    qint64 endMs = 0;
    bool enabled = true;
};

class AudioTimelineRenderer final
{
public:
    static bool renderClip(const QString &inputPath, const QString &outputPath,
                           int sampleRate, int targetSamples, double rate,
                           AudioRenderResult *result = nullptr,
                           QString *error = nullptr);

    static bool assemble(const QVector<QString> &clipPaths,
                         const QVector<AudioTimelinePlacement> &placements,
                         const QString &outputPath, int sampleRate,
                         QString *error = nullptr);
};

} // namespace LAStudio
