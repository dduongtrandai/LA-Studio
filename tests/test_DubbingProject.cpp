#include "test_DubbingProject.h"

#include "dubbing/DubbingProject.h"
#include "controllers/dubbing/DubbingController.h"
#include "controllers/dubbing/DubbingJobRunner.h"
#include "controllers/dubbing/DubbingTranslationFixService.h"
#include "dubbing/AlignmentRefinementService.h"
#include "dubbing/DubbingSegmentNormalizer.h"
#include "dubbing/DubbingDuration.h"
#include "dubbing/DubbingVoiceReferenceSelector.h"
#include "dubbing/media/AtomicMediaCommit.h"
#include "controllers/app/AppController.h"
#include "audio/WavIO.h"
#include "stt/SttEngine.h"
#include "tts/TtsEngine.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace LAStudio {

void TestDubbingProject::normalizesLmStudioTranslationFixConfiguration()
{
    const QVariantMap config =
        DubbingTranslationFixService::normalizedConfiguration({
            {QStringLiteral("provider"), QStringLiteral("api")},
            {QStringLiteral("configured"), true},
            {QStringLiteral("serverUrl"),
             QStringLiteral(" http://127.0.0.1:1234/v1/chat/completions ")},
            {QStringLiteral("model"), QStringLiteral(" qwen3.5-2b ")},
            {QStringLiteral("runtimeId"), QStringLiteral(" llama-win-x86_64-cuda-12.4 ")},
            {QStringLiteral("runtimeVersion"), QStringLiteral(" b10036 ")},
            {QStringLiteral("selectedFiles"),
             QVariantMap{{QStringLiteral("model"), QStringLiteral("Qwen3.5-2B-Q8_0.gguf")}}},
            {QStringLiteral("maxAttempts"), 99},
            {QStringLiteral("temperature"), 4.0}
        });
    QCOMPARE(config.value(QStringLiteral("serverUrl")).toString(),
             QStringLiteral("http://127.0.0.1:1234/v1/chat/completions"));
    QCOMPARE(config.value(QStringLiteral("provider")).toString(), QStringLiteral("api"));
    QVERIFY(config.value(QStringLiteral("configured")).toBool());
    QCOMPARE(config.value(QStringLiteral("model")).toString(),
             QStringLiteral("qwen3.5-2b"));
    QCOMPARE(config.value(QStringLiteral("runtimeId")).toString(),
             QStringLiteral("llama-win-x86_64-cuda-12.4"));
    QCOMPARE(config.value(QStringLiteral("runtimeVersion")).toString(),
             QStringLiteral("b10036"));
    QCOMPARE(config.value(QStringLiteral("selectedFiles")).toMap()
                 .value(QStringLiteral("model")).toString(),
             QStringLiteral("Qwen3.5-2B-Q8_0.gguf"));
    QCOMPARE(config.value(QStringLiteral("maxAttempts")).toInt(), 8);
    QCOMPARE(config.value(QStringLiteral("temperature")).toDouble(), 1.5);
    QCOMPARE(
        DubbingTranslationFixService::chatUrl(
            config.value(QStringLiteral("serverUrl")).toString()).toString(),
        QStringLiteral("http://127.0.0.1:1234/api/v1/chat"));
    QCOMPARE(
        DubbingTranslationFixService::modelsUrl(
            config.value(QStringLiteral("serverUrl")).toString()).toString(),
        QStringLiteral("http://127.0.0.1:1234/api/v1/models"));

    const QVariantMap invalidProvider =
        DubbingTranslationFixService::normalizedConfiguration({
            {QStringLiteral("provider"), QStringLiteral("unknown")}
        });
    QCOMPARE(invalidProvider.value(QStringLiteral("provider")).toString(),
             QStringLiteral("lmstudio"));
    QVERIFY(!invalidProvider.value(QStringLiteral("configured")).toBool());
}

void TestDubbingProject::parsesLmStudioTranslationFixResponses()
{
    QCOMPARE(
        DubbingTranslationFixService::cleanAssistantText(
            QStringLiteral("<think>internal reasoning</think>\n"
                           "Bản dịch: \"Một câu đã sửa.\"")),
        QStringLiteral("Một câu đã sửa."));
    QCOMPARE(
        DubbingTranslationFixService::cleanAssistantText(
            QStringLiteral("```text\nMột câu khác.\n```")),
        QStringLiteral("Một câu khác."));
}

void TestDubbingProject::fixesOnlyTranslationsOverPhonemeLimit()
{
    const QString text = QStringLiteral("Đây là một câu dịch để kiểm tra.");
    const int phonemes =
        DubbingDurationPlanner::countPhonemes(text, QStringLiteral("vi"));
    QVERIFY(phonemes > 1);

    const QVariantMap overBudget{
        {QStringLiteral("targetText"), text},
        {QStringLiteral("durationBudget"),
         QVariantMap{{QStringLiteral("minUnits"), 1},
                     {QStringLiteral("maxUnits"), phonemes - 1}}}};
    const QVariantMap underBudget{
        {QStringLiteral("targetText"), text},
        {QStringLiteral("durationBudget"),
         QVariantMap{{QStringLiteral("minUnits"), phonemes + 1},
                     {QStringLiteral("maxUnits"), phonemes + 5}}}};
    const QVariantMap withinBudget{
        {QStringLiteral("targetText"), text},
        {QStringLiteral("durationBudget"),
         QVariantMap{{QStringLiteral("minUnits"), phonemes},
                     {QStringLiteral("maxUnits"), phonemes}}}};

    QCOMPARE(DubbingTranslationFixService::eligibleSegmentCount(
                 {overBudget, underBudget, withinBudget}, QStringLiteral("vi")),
             1);
}

void TestDubbingProject::ranksPartialTranslationFixesByBudgetDistance()
{
    QVERIFY(DubbingTranslationFixService::isCloserToBudget(97, 59, 47, 57));
    QVERIFY(DubbingTranslationFixService::isCloserToBudget(68, 60, 47, 57));
    QVERIFY(!DubbingTranslationFixService::isCloserToBudget(68, 80, 47, 57));
    QVERIFY(!DubbingTranslationFixService::isCloserToBudget(59, 45, 47, 57));
    QVERIFY(DubbingTranslationFixService::isCloserToBudget(59, 55, 47, 57));
}

void TestDubbingProject::roundTripsVersionedJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DubbingProject original;
    original.projectPath = dir.filePath(QStringLiteral("demo.ladub.json"));
    original.sourceMediaPath = QStringLiteral("C:/media/demo.mp4");
    original.sourceLanguage = QStringLiteral("en");
    original.targetLanguage = QStringLiteral("vi");
    original.dubbingQuality = QStringLiteral("adaptive");
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
    QCOMPARE(loaded.dubbingQuality, QStringLiteral("adaptive"));
    QCOMPARE(loaded.segments.size(), 1);
    QCOMPARE(loaded.segments.first().toMap().value(QStringLiteral("startMs")).toLongLong(), qint64(1000));
}

void TestDubbingProject::migratesLegacyProjectsToLlmRewritePipeline()
{
    DubbingProject migrated;
    QString error;
    const QJsonObject legacy{
        {QStringLiteral("schemaVersion"), 3},
        {QStringLiteral("durationControl"),
         QJsonObject{{QStringLiteral("enabled"), true},
                     {QStringLiteral("autoRewrite"), false}}}
    };
    QVERIFY2(DubbingProject::fromJson(legacy, migrated, &error), qPrintable(error));
    QVERIFY(migrated.durationControl.value(QStringLiteral("autoRewrite")).toBool());
    QCOMPARE(migrated.dubbingQuality, QStringLiteral("fast"));

    DubbingProject current;
    const QJsonObject explicitOptOut{
        {QStringLiteral("schemaVersion"), DubbingProject::CurrentSchemaVersion},
        {QStringLiteral("durationControl"),
         QJsonObject{{QStringLiteral("enabled"), true},
                     {QStringLiteral("autoRewrite"), false}}}
    };
    QVERIFY2(DubbingProject::fromJson(explicitOptOut, current, &error), qPrintable(error));
    QVERIFY(!current.durationControl.value(QStringLiteral("autoRewrite")).toBool());
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

void TestDubbingProject::automaticWorkflowLocksSettingsUntilPaused()
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
    QVERIFY(controller.startAutomaticWorkflow(dir.filePath(QStringLiteral("dubbed.mp4"))));
    QVERIFY(controller.automaticSetupActive());
    QVERIFY(controller.processing());
    QVERIFY(controller.settingsLocked());
    QCOMPARE(controller.workflowMode(), QStringLiteral("automatic"));
    QCOMPARE(controller.currentStepId(), QStringLiteral("import"));
    for (const QVariant &value : controller.workflowNodes()) {
        QVERIFY2(value.toMap().value(QStringLiteral("state")).toString()
                     != QStringLiteral("running"),
                 "Model preparation must not be presented as an executing workflow node");
    }
    QVERIFY(!controller.automaticEvents().isEmpty());

    controller.pauseAutomaticWorkflow();
    QVERIFY(!controller.automaticSetupActive());
    QVERIFY(!controller.processing());
    QVERIFY(!controller.settingsLocked());
    QCOMPARE(controller.workflowMode(), QStringLiteral("paused"));
    QVERIFY(controller.automaticStatusText().contains(QStringLiteral("Paused")));
}

void TestDubbingProject::sourceSeparationExposesModelSelection()
{
    DubbingController controller(nullptr, nullptr);
    QVariantMap sourceSeparationNode;
    for (const QVariant &value : controller.workflowNodes()) {
        const QVariantMap node = value.toMap();
        if (node.value(QStringLiteral("id")).toString() == QStringLiteral("source-separate")) {
            sourceSeparationNode = node;
            break;
        }
    }

    QVERIFY(!sourceSeparationNode.isEmpty());
    QVERIFY(sourceSeparationNode.value(QStringLiteral("configurable")).toBool());
    QCOMPARE(sourceSeparationNode.value(QStringLiteral("capabilityId")).toString(),
             QStringLiteral("voice-isolation"));
}

void TestDubbingProject::targetLanguageUpdatesVoiceNodeLanguage()
{
    DubbingController controller(nullptr, nullptr);
    QVERIFY(controller.setWorkflowNodeParameters(
        QStringLiteral("synthesize"),
        QVariantMap{{QStringLiteral("lang"), QStringLiteral("en")}}));

    controller.setTargetLanguage(QStringLiteral("ja"));

    QCOMPARE(controller.targetLanguage(), QStringLiteral("ja"));
    const QVariantMap synthesis = controller.workflowNodeConfigurations()
                                      .value(QStringLiteral("synthesize")).toMap();
    QCOMPARE(synthesis.value(QStringLiteral("parameters")).toMap()
                 .value(QStringLiteral("lang")).toString(),
             QStringLiteral("ja"));
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

void TestDubbingProject::audioGenerationWaitsForCompletedSynthesis()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TtsEngine tts;
    tts.loadModel(QStringLiteral("mock-model.onnx"));
    DubbingJobRunner runner(nullptr, &tts);
    QSignalSpy completedSpy(&runner, &DubbingJobRunner::stageCompleted);
    QSignalSpy errorSpy(&runner, &DubbingJobRunner::errorOccurred);

    const QVariantList segments{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("s1")},
                    {QStringLiteral("startMs"), 0},
                    {QStringLiteral("endMs"), 1000},
                    {QStringLiteral("targetText"), QStringLiteral("Xin chào")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("s2")},
                    {QStringLiteral("startMs"), 1000},
                    {QStringLiteral("endMs"), 2000},
                    {QStringLiteral("targetText"), QStringLiteral("Thế giới")}}
    };
    runner.startAudioGeneration(segments, dir.filePath(QStringLiteral("project.ladub.json")));

    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.size(), 1, 3000);
    QCOMPARE(errorSpy.size(), 0);
    QVERIFY(!runner.processing());

    const QVariantMap outputs = completedSpy.constFirst().at(1).toMap();
    const QVariantList timeline = outputs.value(QStringLiteral("timeline")).toList();
    QCOMPARE(timeline.size(), 2);
    for (const QVariant &entry : timeline) {
        const QVariantMap segment = entry.toMap();
        QVERIFY(!segment.value(QStringLiteral("clipPath")).toString().isEmpty());
        QVERIFY(QFileInfo::exists(segment.value(QStringLiteral("clipPath")).toString()));
        QVERIFY(!segment.value(QStringLiteral("waveformSamples")).toList().isEmpty());
    }
}

void TestDubbingProject::audioGenerationUsesSelectedVoiceForEverySegment()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TtsEngine tts;
    tts.loadModel(QStringLiteral("mock-model.onnx"));
    DubbingJobRunner runner(nullptr, &tts);
    QSignalSpy completedSpy(&runner, &DubbingJobRunner::stageCompleted);
    QSignalSpy errorSpy(&runner, &DubbingJobRunner::errorOccurred);

    const QVariantList segments{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("s1")},
                    {QStringLiteral("startMs"), 0},
                    {QStringLiteral("endMs"), 10},
                    {QStringLiteral("targetText"), QStringLiteral("Một")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("s2")},
                    {QStringLiteral("startMs"), 10},
                    {QStringLiteral("endMs"), 20},
                    {QStringLiteral("targetText"), QStringLiteral("Hai")}}
    };
    runner.startAudioGeneration(
        segments,
        dir.filePath(QStringLiteral("project.ladub.json")),
        QVariantMap{{QStringLiteral("voice"), QStringLiteral("preset-a")}});

    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.size(), 1, 3000);
    QCOMPARE(errorSpy.size(), 0);
    const QVariantList timeline = completedSpy.constFirst().at(1).toMap()
                                      .value(QStringLiteral("timeline")).toList();
    QCOMPARE(timeline.size(), 2);
    for (const QVariant &entry : timeline) {
        const WavIO::WavData wav = WavIO::loadAsFloat(
            entry.toMap().value(QStringLiteral("clipPath")).toString());
        QVERIFY(!wav.samples.isEmpty());
        QVERIFY(qAbs(wav.samples.first() - 0.2f) < 0.001f);
    }
}

void TestDubbingProject::selectsBestAutomaticVoiceReference()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    constexpr int sampleRate = 24000;
    QVector<float> samples(sampleRate * 6);
    for (int i = 0; i < sampleRate * 3; ++i) samples[i] = 1.0f;
    constexpr double pi = 3.14159265358979323846;
    for (int i = sampleRate * 3; i < samples.size(); ++i)
        samples[i] = 0.1f * qSin(2.0 * pi * 220.0 * i / sampleRate);
    const QString sourcePath = dir.filePath(QStringLiteral("source.wav"));
    QVERIFY(WavIO::saveFloat(sourcePath, samples.constData(), samples.size(), sampleRate));

    const QVariantList segments{
        QVariantMap{{QStringLiteral("startMs"), 0},
                    {QStringLiteral("endMs"), 3000},
                    {QStringLiteral("sourceText"), QStringLiteral("Clipped speech")}},
        QVariantMap{{QStringLiteral("startMs"), 3000},
                    {QStringLiteral("endMs"), 6000},
                    {QStringLiteral("sourceText"), QStringLiteral("Clean speech")}}
    };
    const QString projectPath = dir.filePath(QStringLiteral("project.ladub.json"));
    const DubbingVoiceReference selected =
        DubbingVoiceReferenceSelector::select(sourcePath, segments, projectPath);

    QVERIFY2(selected.isValid(), qPrintable(selected.error));
    QCOMPARE(selected.startMs, qint64(3000));
    QCOMPARE(selected.endMs, qint64(6000));
    QCOMPARE(selected.referenceText, QStringLiteral("Clean speech"));
    QVERIFY(QFileInfo::exists(selected.audioPath));
}

void TestDubbingProject::audioGenerationUsesAutomaticVoiceReference()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    constexpr int sampleRate = 24000;
    QVector<float> source(sampleRate * 4);
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < source.size(); ++i)
        source[i] = 0.1f * qSin(2.0 * pi * 180.0 * i / sampleRate);
    const QString sourcePath = dir.filePath(QStringLiteral("source.wav"));
    QVERIFY(WavIO::saveFloat(sourcePath, source.constData(), source.size(), sampleRate));

    TtsEngine tts;
    tts.loadModel(QStringLiteral("mock-model.onnx"));
    DubbingJobRunner runner(nullptr, &tts);
    QSignalSpy completedSpy(&runner, &DubbingJobRunner::stageCompleted);
    QSignalSpy errorSpy(&runner, &DubbingJobRunner::errorOccurred);
    const QVariantList segments{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("s1")},
                    {QStringLiteral("startMs"), 0},
                    {QStringLiteral("endMs"), 4000},
                    {QStringLiteral("sourceText"), QStringLiteral("Original reference words")},
                    {QStringLiteral("targetText"), QStringLiteral("Translated speech")}}
    };
    runner.startAudioGeneration(
        segments, dir.filePath(QStringLiteral("project.ladub.json")),
        QVariantMap{{QStringLiteral("autoSelectVoiceReference"), true},
                    {QStringLiteral("autoReferenceSourcePath"), sourcePath}});

    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.size(), 1, 3000);
    QCOMPARE(errorSpy.size(), 0);
    const QVariantMap generated = completedSpy.constFirst().at(1).toMap()
                                      .value(QStringLiteral("timeline")).toList().first().toMap();
    QCOMPARE(generated.value(QStringLiteral("voiceReferenceText")).toString(),
             QStringLiteral("Original reference words"));
    QVERIFY(QFileInfo::exists(generated.value(QStringLiteral("voiceReferencePath")).toString()));
    QCOMPARE(tts.lastGenerationMode(), QStringLiteral("voice-cloning"));
}

void TestDubbingProject::audioMixRunsAsynchronously()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    TtsEngine tts;
    tts.loadModel(QStringLiteral("mock-model.onnx"));
    DubbingJobRunner runner(nullptr, &tts);
    QSignalSpy completedSpy(&runner, &DubbingJobRunner::stageCompleted);

    const QVariantList segments{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("s1")},
                    {QStringLiteral("startMs"), 0},
                    {QStringLiteral("endMs"), 1000},
                    {QStringLiteral("targetText"), QStringLiteral("Xin chao")}}
    };
    runner.startAudioGeneration(segments, dir.filePath(QStringLiteral("project.ladub.json")));
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.size(), 1, 3000);

    const QVariantList timeline = completedSpy.constFirst().at(1).toMap()
                                      .value(QStringLiteral("timeline")).toList();
    const QString previewPath = dir.filePath(QStringLiteral("preview.wav"));
    QVERIFY(runner.renderPreview(timeline,
                                 dir.filePath(QStringLiteral("project.ladub.json")),
                                 previewPath));
    QVERIFY(runner.processing());
    QTRY_COMPARE_WITH_TIMEOUT(completedSpy.size(), 2, 3000);
    QCOMPARE(completedSpy.at(1).at(0).toString(), QStringLiteral("mix"));
    QVERIFY(QFileInfo::exists(previewPath));
    QVERIFY(!runner.processing());
}

void TestDubbingProject::commitsMediaExportAtomically()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString destination = dir.filePath(QStringLiteral("export.mp4"));
    const QString staging = dir.filePath(QStringLiteral("export.staging"));
    QFile oldFile(destination);
    QVERIFY(oldFile.open(QIODevice::WriteOnly));
    QVERIFY(oldFile.write("old") == 3);
    oldFile.close();
    QFile stagedFile(staging);
    QVERIFY(stagedFile.open(QIODevice::WriteOnly));
    QVERIFY(stagedFile.write("new") == 3);
    stagedFile.close();

    QString error;
    QVERIFY2(AtomicMediaCommit::commit(staging, destination, &error), qPrintable(error));
    QFile result(destination);
    QVERIFY(result.open(QIODevice::ReadOnly));
    QCOMPARE(result.readAll(), QByteArray("new"));
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

void TestDubbingProject::unchangedTextEditPreservesTranslationMetadata()
{
    DubbingController controller(nullptr, nullptr);
    controller.addSegment(0, 1000, QStringLiteral("Hello"));
    controller.updateSegment(
        0, QVariantMap{{QStringLiteral("targetText"), QStringLiteral("Xin chao")}});
    controller.updateSegment(0, QVariantMap{
        {QStringLiteral("state"), QStringLiteral("translated")},
        {QStringLiteral("durationBudget"),
         QVariantMap{{QStringLiteral("targetUnits"), 8},
                     {QStringLiteral("minUnits"), 6},
                     {QStringLiteral("maxUnits"), 10}}},
        {QStringLiteral("durationUnits"), 8},
        {QStringLiteral("durationStatus"), QStringLiteral("within-budget")}
    });

    controller.updateSegment(0, QVariantMap{{QStringLiteral("sourceText"), QStringLiteral("Hello")}});
    controller.updateSegment(0, QVariantMap{{QStringLiteral("targetText"), QStringLiteral("Xin chao")}});

    const QVariantMap updated = controller.segments().at(0).toMap();
    QCOMPARE(updated.value(QStringLiteral("state")).toString(), QStringLiteral("translated"));
    QCOMPARE(updated.value(QStringLiteral("durationUnits")).toInt(), 8);
    QCOMPARE(updated.value(QStringLiteral("durationStatus")).toString(),
             QStringLiteral("within-budget"));
    QVERIFY(updated.contains(QStringLiteral("durationBudget")));
}

void TestDubbingProject::targetTextEditRefreshesDurationMetadata()
{
    DubbingController controller(nullptr, nullptr);
    controller.setTargetLanguage(QStringLiteral("vi"));
    controller.addSegment(0, 1000, QStringLiteral("Hello"));
    const QString original = QStringLiteral("Xin chao");
    const QString replacement = QStringLiteral("Xin chao ban");
    const int replacementUnits = DubbingDurationPlanner::countPhonemes(
        replacement, QStringLiteral("vi"));
    QVERIFY(replacementUnits > 0);
    controller.updateSegment(0, QVariantMap{
        {QStringLiteral("targetText"), original},
        {QStringLiteral("durationBudget"),
         QVariantMap{{QStringLiteral("targetUnits"), replacementUnits},
                     {QStringLiteral("minUnits"), replacementUnits},
                     {QStringLiteral("maxUnits"), replacementUnits}}},
        {QStringLiteral("durationUnits"), 1},
        {QStringLiteral("durationStatus"), QStringLiteral("needs-review")}
    });

    controller.updateSegment(
        0, QVariantMap{{QStringLiteral("targetText"), replacement}});

    const QVariantMap updated = controller.segments().at(0).toMap();
    QCOMPARE(updated.value(QStringLiteral("state")).toString(), QStringLiteral("stale"));
    QCOMPARE(updated.value(QStringLiteral("durationUnits")).toInt(), replacementUnits);
    QCOMPARE(updated.value(QStringLiteral("durationStatus")).toString(),
             QStringLiteral("within-budget"));
}

void TestDubbingProject::exportsSubtitlesAndReviewPackage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DubbingController controller(nullptr, nullptr);
    const QString projectPath = dir.filePath(QStringLiteral("demo.ladub.json"));
    QVERIFY(controller.newProject(projectPath));
    controller.addSegment(1200, 3450, QStringLiteral("Hello"));
    controller.updateSegment(0, QVariantMap{{QStringLiteral("targetText"), QStringLiteral("Xin chao")}});

    const QString subtitlePath = dir.filePath(QStringLiteral("dubbed.srt"));
    QVERIFY(controller.exportSubtitles(subtitlePath, true));
    QFile subtitleFile(subtitlePath);
    QVERIFY(subtitleFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString subtitle = QString::fromUtf8(subtitleFile.readAll());
    QVERIFY(subtitle.contains(QStringLiteral("00:00:01,200 --> 00:00:03,450")));
    QVERIFY(subtitle.contains(QStringLiteral("Xin chao")));

    const QString packagePath = dir.filePath(QStringLiteral("review-package"));
    QVERIFY(controller.exportPackage(packagePath));
    QVERIFY(QFileInfo(packagePath + QStringLiteral("/manifest.json")).isFile());
    QVERIFY(QFileInfo(packagePath + QStringLiteral("/project.ladub.json")).isFile());
    QVERIFY(QFileInfo(packagePath + QStringLiteral("/source.srt")).isFile());
    QVERIFY(QFileInfo(packagePath + QStringLiteral("/dubbed.srt")).isFile());
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
        QVariantMap{{QStringLiteral("text"), QStringLiteral("sentence")}, {QStringLiteral("startMs"), 950}, {QStringLiteral("endMs"), 2300}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("Second")}, {QStringLiteral("startMs"), 4000}, {QStringLiteral("endMs"), 5200}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("long")}, {QStringLiteral("startMs"), 5250}, {QStringLiteral("endMs"), 6500}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("sentence")}, {QStringLiteral("startMs"), 6550}, {QStringLiteral("endMs"), 8000}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("ends")}, {QStringLiteral("startMs"), 8050}, {QStringLiteral("endMs"), 9300}}
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

void TestDubbingProject::segmentNormalizerRebuildsAcrossAsrBoundaries()
{
    const QVariantList firstWords{
        QVariantMap{{QStringLiteral("text"), QStringLiteral("This")}, {QStringLiteral("startMs"), 0}, {QStringLiteral("endMs"), 600}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("war")}, {QStringLiteral("startMs"), 650}, {QStringLiteral("endMs"), 1200}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("lasted")}, {QStringLiteral("startMs"), 1250}, {QStringLiteral("endMs"), 1900}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("seven")}, {QStringLiteral("startMs"), 1950}, {QStringLiteral("endMs"), 2600}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("hundred")}, {QStringLiteral("startMs"), 2650}, {QStringLiteral("endMs"), 3400}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("eighty")}, {QStringLiteral("startMs"), 3450}, {QStringLiteral("endMs"), 4200}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("years.")}, {QStringLiteral("startMs"), 4250}, {QStringLiteral("endMs"), 5000}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("At")}, {QStringLiteral("startMs"), 5050}, {QStringLiteral("endMs"), 5400}}
    };
    const QVariantList secondWords{
        QVariantMap{{QStringLiteral("text"), QStringLiteral("number")}, {QStringLiteral("startMs"), 5450}, {QStringLiteral("endMs"), 5900}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("five,")}, {QStringLiteral("startMs"), 5950}, {QStringLiteral("endMs"), 6400}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("it")}, {QStringLiteral("startMs"), 6450}, {QStringLiteral("endMs"), 6800}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("is")}, {QStringLiteral("startMs"), 6850}, {QStringLiteral("endMs"), 7200}},
        QVariantMap{{QStringLiteral("text"), QStringLiteral("Vietnam.")}, {QStringLiteral("startMs"), 7250}, {QStringLiteral("endMs"), 8000}}
    };
    const QVariantList input{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("asr-1")},
                    {QStringLiteral("startMs"), 0}, {QStringLiteral("endMs"), 5400},
                    {QStringLiteral("sourceText"), QStringLiteral("This war lasted seven hundred eighty years. At")},
                    {QStringLiteral("words"), firstWords}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("asr-2")},
                    {QStringLiteral("startMs"), 5450}, {QStringLiteral("endMs"), 8000},
                    {QStringLiteral("sourceText"), QStringLiteral("number five, it is Vietnam.")},
                    {QStringLiteral("words"), secondWords}}
    };

    const QVariantList normalized = DubbingSegmentNormalizer::normalize(input);
    QCOMPARE(normalized.size(), 2);
    QCOMPARE(normalized.at(0).toMap().value(QStringLiteral("sourceText")).toString(),
             QStringLiteral("This war lasted seven hundred eighty years."));
    QVERIFY(normalized.at(1).toMap().value(QStringLiteral("sourceText")).toString()
                .startsWith(QStringLiteral("At number five")));
}

void TestDubbingProject::countsVietnameseSyllablesAndPlansBudget()
{
    QCOMPARE(DubbingDurationPlanner::countVietnameseSyllables(QStringLiteral("Xin chào, thế giới!")), 4);
    const QVariantMap segment{{QStringLiteral("startMs"), 0}, {QStringLiteral("endMs"), 2000}};
    const DubbingSpeechBudget budget = DubbingDurationPlanner::plan(segment, 10.0);
    QCOMPARE(budget.slotMs, qint64(2000));
    QVERIFY(budget.targetUnits > 0);
    QVERIFY(budget.minUnits <= budget.targetUnits);
    QVERIFY(budget.targetUnits <= budget.maxUnits);

    DubbingDurationSettings asymmetric;
    asymmetric.lowerToleranceRatio = 0.25;
    asymmetric.upperToleranceRatio = 0.50;
    const DubbingSpeechBudget asymmetricBudget =
        DubbingDurationPlanner::plan(segment, 10.0, asymmetric);
    QCOMPARE(asymmetricBudget.targetUnits, 20);
    QCOMPARE(asymmetricBudget.minUnits, 15);
    QCOMPARE(asymmetricBudget.maxUnits, 30);
}

void TestDubbingProject::selectsImprovingDurationCandidate()
{
    const QString reference = QStringLiteral("Cuoc chien keo dai 100 nam");
    const QString current =
        QStringLiteral("Cuoc chien nay da keo dai trong suot 100 nam");
    const int predicted = DubbingDurationPlanner::countPhonemes(
        reference, QStringLiteral("vi"));
    const QString selected = DubbingDurationPlanner::selectBestCandidate(
        QStringLiteral("The war lasted 100 years."),
        reference,
        current,
        {QStringLiteral("Cuoc chien keo dai 100 nam"),
         QStringLiteral("Cuoc chien rat dai"),
         QStringLiteral("Ban dich dai hon rat nhieu va van co 100 nam")},
        predicted,
        predicted,
        predicted,
        {QStringLiteral("100")},
        QStringLiteral("vi"));
    QCOMPARE(selected, reference);
    QVERIFY(DubbingDurationPlanner::phonemeDistance(
                selected, predicted, QStringLiteral("vi"))
            < DubbingDurationPlanner::phonemeDistance(
                current, predicted, QStringLiteral("vi")));
}

void TestDubbingProject::prefersWithinBudgetDurationCandidate()
{
    const QString reference =
        QStringLiteral("Cuoc chien nay da keo dai trong suot 100 nam");
    const QString withinBudget = QStringLiteral("Cuoc chien keo dai 100 nam");
    const QString closerToReference = QStringLiteral("Cuoc chien nay keo dai 100 nam");
    const int predicted = DubbingDurationPlanner::countPhonemes(
        withinBudget, QStringLiteral("vi"));

    const QString selected = DubbingDurationPlanner::selectBestCandidate(
        QStringLiteral("The war lasted 100 years."),
        reference,
        reference,
        {closerToReference, withinBudget},
        predicted,
        predicted,
        predicted,
        {QStringLiteral("100")},
        QStringLiteral("vi"));

    QCOMPARE(selected, withinBudget);
}

void TestDubbingProject::prefersClosestRepairCandidateOutsideBudget()
{
    const QString reference = QStringLiteral(
        "Cuoc chien nay da keo dai trong suot mot tram nam va gay ra nhieu ton that");
    const QString semanticButLong =
        QStringLiteral("Cuoc chien nay keo dai trong suot mot tram nam");
    const QString closest = QStringLiteral("Chien tranh tram nam");
    const int predicted = DubbingDurationPlanner::countPhonemes(
                              closest, QStringLiteral("vi"))
        + 1;

    const QString selected = DubbingDurationPlanner::selectBestCandidate(
        QStringLiteral("The war lasted one hundred years."),
        reference,
        reference,
        {semanticButLong, closest},
        predicted,
        predicted,
        predicted,
        {},
        QStringLiteral("vi"));

    QCOMPARE(selected, closest);
}

void TestDubbingProject::buildsPauseAlignedTtsChunks()
{
    const QVariantList pauses{
        QVariantMap{{QStringLiteral("kind"), QStringLiteral("leading")},
                    {QStringLiteral("durationMs"), 100}},
        QVariantMap{{QStringLiteral("kind"), QStringLiteral("internal")},
                    {QStringLiteral("durationMs"), 450}},
        QVariantMap{{QStringLiteral("kind"), QStringLiteral("trailing")},
                    {QStringLiteral("durationMs"), 200}}};
    const QVariantList chunks = DubbingDurationPlanner::pauseChunks(
        QStringLiteral("Xin chao [[PAUSE]] the gioi"), pauses);
    QCOMPARE(chunks.size(), 2);
    QCOMPARE(
        chunks.at(0).toMap().value(QStringLiteral("leadingPauseMs")).toLongLong(),
        qint64(100));
    QCOMPARE(
        chunks.at(0).toMap().value(QStringLiteral("pauseAfterMs")).toLongLong(),
        qint64(450));
    QCOMPARE(
        chunks.at(1).toMap().value(QStringLiteral("pauseAfterMs")).toLongLong(),
        qint64(200));
}

void TestDubbingProject::extractsAlignedPauses()
{
    const QVariantMap segment{{QStringLiteral("startMs"), 0}, {QStringLiteral("endMs"), 3000},
        {QStringLiteral("words"), QVariantList{
            QVariantMap{{QStringLiteral("startMs"), 100}, {QStringLiteral("endMs"), 500}},
            QVariantMap{{QStringLiteral("startMs"), 1100}, {QStringLiteral("endMs"), 1500}},
            QVariantMap{{QStringLiteral("startMs"), 1600}, {QStringLiteral("endMs"), 1900}}
        }}};
    const QVariantList pauses = DubbingDurationPlanner::extractPauses(segment);
    QCOMPARE(pauses.size(), 2);
    QCOMPARE(pauses.at(0).toMap().value(QStringLiteral("durationMs")).toLongLong(), qint64(600));
}

void TestDubbingProject::roundTripsDurationSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    DubbingProject project;
    project.projectPath = dir.filePath(QStringLiteral("duration.ladub.json"));
    project.durationControl.insert(QStringLiteral("enabled"), true);
    project.durationControl.insert(QStringLiteral("maxPreTtsIterations"), 3);
    QString error;
    QVERIFY2(project.save(&error), qPrintable(error));
    DubbingProject loaded;
    QVERIFY2(DubbingProject::load(project.projectPath, loaded, &error), qPrintable(error));
    QCOMPARE(loaded.durationControl.value(QStringLiteral("maxPreTtsIterations")).toInt(), 3);
}


} // namespace LAStudio
