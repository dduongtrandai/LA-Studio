#pragma once

#include <QVariantList>

namespace LAStudio {

class DubbingSegmentNormalizer final
{
public:
    // Split long transcript regions into dubbing-friendly segments. Word-level
    // timestamps are used when available; otherwise timings are interpolated
    // inside the original ASR segment and labelled accordingly.
    static QVariantList normalize(const QVariantList &segments);
};

} // namespace LAStudio
