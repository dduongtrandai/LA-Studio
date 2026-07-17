#pragma once

#include <QObject>

namespace LAStudio {

class TestSubtitleVoice final : public QObject
{
    Q_OBJECT
private slots:
    void parsesAndPreservesTimeline();
    void preservesFullyCoveredCue();
    void plansSmartFitWithoutOverlap();
};

} // namespace LAStudio
