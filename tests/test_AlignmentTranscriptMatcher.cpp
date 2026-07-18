#include "test_AlignmentTranscriptMatcher.h"
#include "controllers/alignment/AlignmentTranscriptMatcher.h"
#include <QtTest>

namespace LAStudio {
void TestAlignmentTranscriptMatcher::normalizesUnicodeAndPunctuation()
{
    QCOMPARE(AlignmentTranscriptMatcher::normalize(QString::fromUtf8("  Hôm nay, AI!  ")),
             QString::fromUtf8("hôm nay ai"));
}

void TestAlignmentTranscriptMatcher::matchesMissingWordsMonotonically()
{
    const QStringList lines{QString::fromUtf8("Xin chào tất cả mọi người."),
                            QString::fromUtf8("Hôm nay chúng ta sẽ học về AI.")};
    const QStringList chunks{QString::fromUtf8("xin chào mọi người"),
                             QString::fromUtf8("hôm nay chúng ta học ai")};
    const auto result = AlignmentTranscriptMatcher::match(lines, chunks, QStringLiteral("vie"));
    QVERIFY(result.coverage > 0.65);
    QVERIFY(!result.canonicalTokensByChunk[0].isEmpty());
    QVERIFY(!result.canonicalTokensByChunk[1].isEmpty());
    QVERIFY(result.canonicalTokensByChunk[0].last() < result.canonicalTokensByChunk[1].first());
}

void TestAlignmentTranscriptMatcher::tokenizesCjkByCharacter()
{
    const auto tokens = AlignmentTranscriptMatcher::tokenizeText(QString::fromUtf8("你好，世界"), QStringLiteral("cmn"));
    QCOMPARE(tokens.size(), 4);
}
} // namespace LAStudio
