#include "test_TtsTextPreprocessor.h"

#include "tts/TtsTextPreprocessor.h"

#include <QtTest>

namespace LAStudio {

void TestTtsTextPreprocessor::leavesTextUnchangedWithoutPolicy()
{
    const QString input = QStringLiteral("Hôm nay là 25/12/2023");
    QCOMPARE(TtsTextPreprocessor::prepare(input, {}, {}, nullptr), input);
}

void TestTtsTextPreprocessor::appliesVietNormPolicy()
{
    QVariantMap textNormalization;
    textNormalization.insert(QStringLiteral("policy"), QStringLiteral("vietnorm"));
    textNormalization.insert(QStringLiteral("profile"), QStringLiteral("safe-vi-tts-v1"));
    QVariantMap studioConfig;
    studioConfig.insert(QStringLiteral("textNormalization"), textNormalization);
    QString profile;
    const QString output = TtsTextPreprocessor::prepare(
        QStringLiteral("Hôm nay là 25/12/2023"), studioConfig, {}, &profile);
    QCOMPARE(profile, QStringLiteral("safe-vi-tts-v1"));
    QVERIFY(output.contains(QStringLiteral("hai mươi lăm")));
    QVERIFY(!output.contains(QStringLiteral("25/12/2023")));
}

void TestTtsTextPreprocessor::normalizesVoiceCloningTargetForOmniVoice()
{
    QVariantMap textNormalization;
    textNormalization.insert(QStringLiteral("policy"), QStringLiteral("vietnorm"));
    textNormalization.insert(QStringLiteral("profile"), QStringLiteral("safe-vi-tts-v1"));
    QVariantMap studioConfig;
    studioConfig.insert(QStringLiteral("textNormalization"), textNormalization);

    const QString input = QStringLiteral("Cuộc chiến này kéo dài 781 năm.");
    const QString output = TtsTextPreprocessor::prepare(input, studioConfig, {}, nullptr);
    QVERIFY2(output != input, qPrintable(output));
    QVERIFY(!output.contains(QStringLiteral("781")));
    QVERIFY(output.contains(QStringLiteral("bảy trăm tám mươi mốt")));
}

void TestTtsTextPreprocessor::supportsExplicitSkip()
{
    QVariantMap textNormalization;
    textNormalization.insert(QStringLiteral("policy"), QStringLiteral("vietnorm"));
    QVariantMap studioConfig;
    studioConfig.insert(QStringLiteral("textNormalization"), textNormalization);
    QVariantMap settings;
    settings.insert(QStringLiteral("skip_text_normalization"), true);
    const QString input = QStringLiteral("25% va 14:30");
    QCOMPARE(TtsTextPreprocessor::prepare(input, studioConfig, settings, nullptr), input);

    settings.clear();
    settings.insert(QStringLiteral("skip_normalize"), true);
    QCOMPARE(TtsTextPreprocessor::prepare(input, studioConfig, settings, nullptr), input);
}

} // namespace LAStudio
