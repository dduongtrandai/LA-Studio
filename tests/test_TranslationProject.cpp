#include "test_TranslationProject.h"

#include "translation/TranslationProject.h"

#include <QTemporaryDir>
#include <QtTest>

namespace LAStudio {
void TestTranslationProject::textImportSplitsParagraphsAndRoundTrips()
{
    TranslationProject project;
    QString error;
    QVERIFY2(TranslationProject::importText(QStringLiteral("First paragraph.\n\nSecond paragraph."), project, &error), qPrintable(error));
    QCOMPARE(project.segments.size(), 2);
    QCOMPARE(project.segments.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("segment-1"));
    QTemporaryDir dir;
    project.projectPath = dir.filePath(QStringLiteral("sample.lastudio-translation.json"));
    project.segments[0] = QVariantMap{{QStringLiteral("id"), QStringLiteral("segment-1")}, {QStringLiteral("sourceText"), QStringLiteral("First paragraph.")}, {QStringLiteral("targetText"), QStringLiteral("Doan mot.")}};
    QVERIFY2(project.save(&error), qPrintable(error));
    TranslationProject loaded;
    QVERIFY2(TranslationProject::load(project.projectPath, loaded, &error), qPrintable(error));
    QCOMPARE(loaded.segments.size(), 2);
    QCOMPARE(loaded.segments.first().toMap().value(QStringLiteral("targetText")).toString(), QStringLiteral("Doan mot."));
}

void TestTranslationProject::subtitleImportPreservesTimingAndExportsTargetText()
{
    TranslationProject project;
    QString error;
    const QString srt = QStringLiteral("1\n00:00:01,000 --> 00:00:02,500\nHello\nworld\n\n2\n00:00:03,000 --> 00:00:04,000\nGoodbye");
    QVERIFY2(TranslationProject::importSubtitle(srt, QStringLiteral("srt"), project, &error), qPrintable(error));
    QCOMPARE(project.segments.size(), 2);
    QCOMPARE(project.segments.first().toMap().value(QStringLiteral("startMs")).toLongLong(), qint64(1000));
    QVariantMap first = project.segments.first().toMap(); first.insert(QStringLiteral("targetText"), QStringLiteral("Xin chao")); project.segments[0] = first;
    QVariantMap second = project.segments.last().toMap(); second.insert(QStringLiteral("targetText"), QStringLiteral("Tam biet")); project.segments[1] = second;
    const QString output = project.exportSubtitle(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(output.contains(QStringLiteral("00:00:01,000 --> 00:00:02,500")));
    QVERIFY(output.contains(QStringLiteral("Xin chao")));
}

void TestTranslationProject::rejectsInvalidSubtitleCue()
{
    TranslationProject project;
    QString error;
    QVERIFY(!TranslationProject::importSubtitle(QStringLiteral("1\ninvalid --> timestamp\nText"), QStringLiteral("srt"), project, &error));
    QVERIFY(!error.isEmpty());
}
} // namespace LAStudio
