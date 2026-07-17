#pragma once

#include "TimedTextCue.h"

#include <QVector>
#include <QString>

namespace LAStudio {

struct SubtitleParseResult {
    QVector<TimedTextCue> cues;
    int skippedCues = 0;
    QString error;
    bool ok = false;
};

class SrtTimelineParser final
{
public:
    static SubtitleParseResult parseFile(const QString &path);
    static SubtitleParseResult parseSrt(const QString &content);
};

} // namespace LAStudio
