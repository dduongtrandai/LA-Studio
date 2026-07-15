#include "test_DubbingProject.h"

#include "controllers/DubbingProject.h"
#include "controllers/DubbingController.h"
#include "controllers/DubbingJobRunner.h"
#include "controllers/AlignmentRefinementService.h"
#include "controllers/DubbingSegmentNormalizer.h"
#include "controllers/AppController.h"
#include "stt/SttEngine.h"

#include <QFile>
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

void TestDubbingProject::importingMediaDoesNotStartProcessing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mediaPath = dir.filePath(QStringLiteral("source.mp4"));
    QFile media(mediaPath);
    QVERIFY(media.open(QIODevice::WriteOnly));
    QVERIFY(media.write("video-placeholder") > 0);
    media.close();

    DubbingController controller(nullptr, nullptr);
    QVERIFY(controller.newProject(dir.filePath(QStringLiteral("project.ladub.json"))));
    QVERIFY(controller.importMedia(mediaPath));
    QCOMPARE(controller.sourceMediaPath(), QFileInfo(mediaPath).absoluteFilePath());
    QVERIFY(!controller.processing());
    QVERIFY(controller.normalizedAudioPath().isEmpty());
    QVERIFY(controller.vocalsPath().isEmpty());
    QCOMPARE(controller.workflowMode(), QStringLiteral("idle"));
    QCOMPARE(controller.currentStepId(), QStringLiteral("ingest"));
}

void TestDubbingProject::rejectsRerunningUnsupportedStep()
{
    DubbingController controller(nullptr, nullptr);
    QCOMPARE(controller.workflowMode(), QStringLiteral("idle"));
    QCOMPARE(controller.currentStepId(), QStringLiteral("import"));

    QVERIFY(!controller.rerunStep(QStringLiteral("import")));
    QVERIFY(!controller.rerunStep(QStringLiteral("unknown")));
    QCOMPARE(controller.workflowMode(), QStringLiteral("idle"));
    QCOMPARE(controller.currentStepId(), QStringLiteral("import"));
}

void TestDubbingProject::transcriptionRequiresReadyModel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mediaPath = dir.filePath(QStringLiteral("source.wav"));
    QFile media(mediaPath);
    QVERIFY(media.open(QIODevice::WriteOnly));
    QVERIFY(media.write("audio-placeholder") > 0);
    media.close();

    AppController::instance()->stt()->unloadModel();
    DubbingJobRunner runner(AppController::instance()->sttSession(), nullptr);
    runner.startTranscription(QStringLiteral("en"), mediaPath);

    QVERIFY(!runner.processing());
    QVERIFY(runner.lastError().contains(QStringLiteral("not ready")));
}

void TestDubbingProject::alignmentRefinementFallsBackWithoutDependencies()
{
    const QVariantList input{QVariantMap{{QStringLiteral("id"), QStringLiteral("s1")},
                                         {QStringLiteral("startMs"), 1000},
                                         {QStringLiteral("endMs"), 2000},
                                         {QStringLiteral("sourceText"), QStringLiteral("Hello")}}};
    const AlignmentRefinementResult result = AlignmentRefinementService::refine(
        QStringLiteral("missing-analysis.wav"), QStringLiteral("en"), input, nullptr, nullptr);

    QCOMPARE(result.status, QStringLiteral("skipped"));
    QCOMPARE(result.segments.size(), input.size());
    const QVariantMap fallback = result.segments.first().toMap();
    QCOMPARE(fallback.value(QStringLiteral("sourceText")).toString(), QStringLiteral("Hello"));
    QCOMPARE(fallback.value(QStringLiteral("startMs")).toLongLong(), qint64(1000));
    QCOMPARE(fallback.value(QStringLiteral("timingSource")).toString(), QStringLiteral("asr"));
    QCOMPARE(fallback.value(QStringLiteral("alignmentStatus")).toString(), QStringLiteral("skipped"));
    QVERIFY(!result.attempted);
}

void TestDubbingProject::sourceTextEditInvalidatesWordTiming()
{
    DubbingController controller(nullptr, nullptr);
    controller.addSegment(0, 1000, QStringLiteral("Hello"));
    controller.updateSegment(0, QVariantMap{
        {QStringLiteral("words"), QVariantList{QVariantMap{{QStringLiteral("text"), QStringLiteral("Hello")},
                                                            {QStringLiteral("startMs"), 0},
                                                            {QStringLiteral("endMs"), 500}}}},
        {QStringLiteral("timingSource"), QStringLiteral("ctc")},
        {QStringLiteral("alignmentStatus"), QStringLiteral("aligned")}
    });
    QVERIFY(!controller.segments().at(0).toMap().value(QStringLiteral("words")).toList().isEmpty());

    controller.updateSegment(0, QVariantMap{{QStringLiteral("sourceText"), QStringLiteral("Hi")}});
    const QVariantMap updated = controller.segments().at(0).toMap();
    QVERIFY(updated.value(QStringLiteral("words")).toList().isEmpty());
    QCOMPARE(updated.value(QStringLiteral("timingSource")).toString(), QStringLiteral("asr"));
    QCOMPARE(updated.value(QStringLiteral("alignmentStatus")).toString(), QStringLiteral("pending"));
}

void TestDubbingProject::segmentNormalizerSplitsLongAsrTranscript()
{
    const QVariantList input{QVariantMap{
        {QStringLiteral("id"), QStringLiteral("long-source")},
        {QStringLiteral("startMs"), 320},
        {QStringLiteral("endMs"), 44560},
        {QStringLiteral("sourceText"), QStringLiteral(
            "Iberia is the reconquista where Christian kingdoms in Spain fought Muslim states. "
            "This war lasted seven hundred and eighty one years before the final kingdom fell.")},
        {QStringLiteral("targetText"), QString()},
        {QStringLiteral("speakerId"), QStringLiteral("speaker-1")},
        {QStringLiteral("timingSource"), QStringLiteral("asr")}
    }};

    const QVariantList normalized = DubbingSegmentNormalizer::normalize(input);
    QVERIFY(normalized.size() >= 3);
    QCOMPARE(normalized.constFirst().toMap().value(QStringLiteral("startMs")).toLongLong(), qint64(320));
    QCOMPARE(normalized.constLast().toMap().value(QStringLiteral("endMs")).toLongLong(), qint64(44560));
    qint64 previousEnd = 320;
    for (const QVariant &entry : normalized) {
        const QVariantMap segment = entry.toMap();
        QCOMPARE(segment.value(QStringLiteral("startMs")).toLongLong(), previousEnd);
        QVERIFY(segment.value(QStringLiteral("endMs")).toLongLong() > previousEnd);
        QVERIFY(segment.value(QStringLiteral("endMs")).toLongLong() - previousEnd <= 13000);
        QCOMPARE(segment.value(QStringLiteral("derivedFromSegmentId")).toString(), QStringLiteral("long-source"));
        QCOMPARE(segment.value(QStringLiteral("timingSource")).toString(), QStringLiteral("asr-interpolated"));
        previousEnd = segment.value(QStringLiteral("endMs")).toLongLong();
    }
}

void TestDubbingProject::segmentNormalizerUsesAlignedWordBoundaries()
{
    const QVariantList words{
        QVariantMap{{QStringLiteral("text"), QStringLiteral("First")}, {QStringLiteral("startMs"), 0}, {QStringLiteral("endMs"), 900}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("sentence.")}, {QStringLiteral("startMs"), 950}, {QStringLiteral("endMs"), 2300}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("Second")}, {QStringLiteral("startMs"), 4000}, {QStringLiteral("endMs"), 5200}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("long")}, {QStringLiteral("startMs"), 5250}, {QStringLiteral("endMs"), 6500}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("sentence")}, {QStringLiteral("startMs"), 6550}, {QStringLiteral("endMs"), 8000}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("ends.")}, {QStringLiteral("startMs"), 8050}, {QStringLiteral("endMs"), 9300}}
    };
    const QVariantList input{QVariantMap{{QStringLiteral("id"), QStringLiteral("aligned-source")},
                                         {QStringLiteral("startMs"), 0},
                                         {QStringLiteral("endMs"), 9300},
                                         {QStringLiteral("sourceText"), QStringLiteral("First sentence. Second long sentence ends.")},
                                         {QStringLiteral("words"), words},
                                         {QStringLiteral("timingSource"), QStringLiteral("ctc")}}};

    const QVariantList normalized = DubbingSegmentNormalizer::normalize(input);
    QCOMPARE(normalized.size(), 2);
    QCOMPARE(normalized.at(0).toMap().value(QStringLiteral("endMs")).toLongLong(), qint64(2300));
    QCOMPARE(normalized.at(1).toMap().value(QStringLiteral("startMs")).toLongLong(), qint64(4000));
    QCOMPARE(normalized.at(0).toMap().value(QStringLiteral("sourceText")).toString(), QStringLiteral("First sentence."));
    QCOMPARE(normalized.at(1).toMap().value(QStringLiteral("words")).toList().size(), 4);
}

} // namespace LAStudio
