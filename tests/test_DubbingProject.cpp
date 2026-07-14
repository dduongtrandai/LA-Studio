#include "test_DubbingProject.h"

#include "controllers/DubbingProject.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace LAStudio {

void TestDubbingProject::roundTripsVersionedJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DubbingProject original;
    original.projectPath = dir.filePath(QStringLiteral("demo.ladub.json"));
    original.sourceMediaPath = QStringLiteral("C:/media/demo.mp4");
    original.sourceLanguage = QStringLiteral("en");
    original.targetLanguage = QStringLiteral("vi");
    original.speakers.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("speaker-1")} });
    original.segments.append(QVariantMap{{QStringLiteral("startMs"), 1000},
                                         {QStringLiteral("endMs"), 2400},
                                         {QStringLiteral("sourceText"), QStringLiteral("Hello")} });

    QString error;
    QVERIFY2(original.save(&error), qPrintable(error));
    QVERIFY(QFileInfo::exists(original.projectPath));

    DubbingProject loaded;
    QVERIFY2(DubbingProject::load(original.projectPath, loaded, &error), qPrintable(error));
    QCOMPARE(loaded.sourceMediaPath, original.sourceMediaPath);
    QCOMPARE(loaded.targetLanguage, original.targetLanguage);
    QCOMPARE(loaded.segments.size(), 1);
    QCOMPARE(loaded.segments.first().toMap().value(QStringLiteral("startMs")).toLongLong(), qint64(1000));
}

void TestDubbingProject::rejectsUnknownSchema()
{
    DubbingProject project;
    QString error;
    QVERIFY(!DubbingProject::fromJson(QJsonObject{{QStringLiteral("schemaVersion"), 99}}, project, &error));
    QVERIFY(error.contains(QStringLiteral("Unsupported")));
}

void TestDubbingProject::mergesSegmentPatchesByStableId()
{
    const QVariantList source{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("a")}, {QStringLiteral("startMs"), 10}, {QStringLiteral("endMs"), 15}, {QStringLiteral("speakerId"), QStringLiteral("s1")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("b")}, {QStringLiteral("startMs"), 20}, {QStringLiteral("endMs"), 30}}
    };
    QVariantList merged;
    QString error;
    QVERIFY(DubbingProject::mergeSegmentPatches(source,
        {QVariantMap{{QStringLiteral("id"), QStringLiteral("b")}, {QStringLiteral("targetText"), QStringLiteral("Xin chao")}}},
        merged, &error));
    QCOMPARE(merged.size(), 2);
    QCOMPARE(merged.at(0).toMap().value(QStringLiteral("speakerId")).toString(), QStringLiteral("s1"));
    QCOMPARE(merged.at(1).toMap().value(QStringLiteral("targetText")).toString(), QStringLiteral("Xin chao"));
}

void TestDubbingProject::rejectsUnknownAndDuplicateSegmentPatches()
{
    const QVariantList source{QVariantMap{{QStringLiteral("id"), QStringLiteral("a")}, {QStringLiteral("startMs"), 0}, {QStringLiteral("endMs"), 1}}};
    QVariantList merged;
    QString error;
    QVERIFY(!DubbingProject::mergeSegmentPatches(source,
        {QVariantMap{{QStringLiteral("id"), QStringLiteral("missing")}}}, merged, &error));
    QVERIFY(error.contains(QStringLiteral("unknown")));
    QVERIFY(!DubbingProject::mergeSegmentPatches(source,
        {QVariantMap{{QStringLiteral("id"), QStringLiteral("a")} }, QVariantMap{{QStringLiteral("id"), QStringLiteral("a")}}},
        merged, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate")));
}

} // namespace LAStudio
