#pragma once

#include <QObject>

namespace LAStudio {
class TestTranslationProject final : public QObject {
    Q_OBJECT
private slots:
    void textImportSplitsParagraphsAndRoundTrips();
    void subtitleImportPreservesTimingAndExportsTargetText();
    void rejectsInvalidSubtitleCue();
};
} // namespace LAStudio
