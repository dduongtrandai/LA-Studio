#pragma once

#include <QString>
#include <QVariantList>

namespace LAStudio {

class SttSubtitleService final
{
public:
    // Converts backend segments (seconds or startMs/endMs) to editable cue
    // maps.  Word timings are retained when the backend exposes them.
    static QVariantList cuesFromSegments(const QVariantList &segments,
                                         qint64 durationMs = 0);
    static QString toSrt(const QVariantList &cues);
    static QString toVtt(const QVariantList &cues);
    static bool writeSrt(const QString &path, const QVariantList &cues,
                         QString *error = nullptr);
    static bool writeVtt(const QString &path, const QVariantList &cues,
                         QString *error = nullptr);
};

} // namespace LAStudio
