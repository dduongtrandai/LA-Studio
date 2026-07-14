#pragma once
#include <QObject>

namespace LAStudio {
class TestAlignmentTranscriptMatcher : public QObject {
    Q_OBJECT
private slots:
    void normalizesUnicodeAndPunctuation();
    void matchesMissingWordsMonotonically();
    void tokenizesCjkByCharacter();
};
}
