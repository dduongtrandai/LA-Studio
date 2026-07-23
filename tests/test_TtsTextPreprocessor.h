#pragma once

#include <QObject>

namespace LAStudio {

class TestTtsTextPreprocessor final : public QObject {
    Q_OBJECT

private slots:
    void leavesTextUnchangedWithoutPolicy();
    void appliesVietNormPolicy();
    void normalizesVoiceCloningTargetForOmniVoice();
    void supportsExplicitSkip();
};

} // namespace LAStudio
