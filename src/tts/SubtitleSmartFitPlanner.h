#pragma once

#include "subtitles/TimedTextCue.h"

#include <QVector>
#include <QString>

namespace LAStudio {

struct SubtitleFit {
    qint64 scheduledStartMs = 0;
    qint64 effectiveEndMs = 0;
    qint64 slotMs = 0;
    double audioRate = 1.0;
    qint64 outputMs = 0;
    qint64 overflowMs = 0;
    bool droppedOverlap = false;
    QString status = QStringLiteral("fits");
};

class SubtitleSmartFitPlanner final
{
public:
    static QVector<SubtitleFit> plan(const QVector<TimedTextCue> &cues,
                                     const QVector<qint64> &naturalDurationsMs);
};

} // namespace LAStudio
