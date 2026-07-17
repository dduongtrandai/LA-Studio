#include <QtTest>

#include "test_SubtitleVoice.h"
#include "subtitles/SrtTimelineParser.h"
#include "tts/SubtitleSmartFitPlanner.h"

using namespace LAStudio;

void TestSubtitleVoice::parsesAndPreservesTimeline()
{
    const QString srt = QString::fromUtf8("\xEF\xBB\xBF" "1\n00:00:01,000 --> 00:00:04,000\nFirst\n\n"
                                          "2\n00:00:03,000 --> 00:00:05,000\nSecond\n");
    const SubtitleParseResult result = SrtTimelineParser::parseSrt(srt);
    QVERIFY(result.ok);
    QCOMPARE(result.cues.size(), 2);
    QCOMPARE(result.cues.at(1).startMs, qint64(3000));
}

void TestSubtitleVoice::preservesFullyCoveredCue()
{
    const QString srt = QStringLiteral("1\n00:00:01,000 --> 00:00:10,000\nFirst\n\n"
                                       "2\n00:00:03,000 --> 00:00:08,000\nCovered\n");
    const SubtitleParseResult result = SrtTimelineParser::parseSrt(srt);
    QVERIFY(result.ok);
    QCOMPARE(result.cues.size(), 2);
}

void TestSubtitleVoice::plansSmartFitWithoutOverlap()
{
    const QVector<TimedTextCue> cues{
        TimedTextCue{QStringLiteral("1"), 1, QStringLiteral("First"), 0, 1000},
        TimedTextCue{QStringLiteral("2"), 2, QStringLiteral("Second"), 800, 1200},
        TimedTextCue{QStringLiteral("3"), 3, QStringLiteral("Third"), 800, 900}
    };
    const QVector<SubtitleFit> fits = SubtitleSmartFitPlanner::plan(cues, {1900, 500, 4000});
    QCOMPARE(fits.size(), 3);
    QCOMPARE(fits.at(0).effectiveEndMs, qint64(1000));
    QCOMPARE(fits.at(1).scheduledStartMs, qint64(1000));
    QVERIFY(fits.at(2).droppedOverlap);
    QCOMPARE(fits.at(0).slotMs, qint64(1000));
    QVERIFY(fits.at(0).audioRate > 1.0);
    QCOMPARE(fits.at(1).audioRate, 1.8);
    QVERIFY(fits.at(1).overflowMs > 0);
    for (const SubtitleFit &fit : fits) {
        QVERIFY(fit.outputMs <= fit.slotMs);
    }
}
