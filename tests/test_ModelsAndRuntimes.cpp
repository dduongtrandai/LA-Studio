#include "test_ModelsAndRuntimes.h"
#include <QtTest>
#include <QThreadPool>

#include "core/CapabilityFamilyModel.h"
#include "core/CatalogManager.h"
#include "core/RegistryManager.h"
#include "core/Settings.h"
#include "core/StudioSelectionRepository.h"
#include "core/RuntimeManager.h"
#include "core/ModelManager.h"
#include "core/LogViewService.h"
#include "controllers/models/StudioConfigurationResolver.h"
#include "controllers/models/CapabilitySettingsSchema.h"
#include "runtimes/LlamaTranslationInterface.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace LAStudio {

void TestModelsAndRuntimes::testCapabilitySettingsSchemaPreservesRuntimeVoiceChoices()
{
    const QVariantMap familyConfig{
        {QStringLiteral("studio"), QVariantMap{
            {QStringLiteral("tts"), QVariantMap{
                {QStringLiteral("parameters"), QVariantList{QStringLiteral("voice")}}
            }}
        }},
        {QStringLiteral("parameterDefinitions"), QVariantMap{
            {QStringLiteral("voice"), QVariantMap{
                {QStringLiteral("id"), QStringLiteral("voice")},
                {QStringLiteral("name"), QStringLiteral("Voice")},
                {QStringLiteral("type"), QStringLiteral("choice")}
            }}
        }},
        {QStringLiteral("speakersMetadata"), QVariantList{
            QVariantMap{
                {QStringLiteral("name"), QStringLiteral("speaker_a")},
                {QStringLiteral("displayName"), QStringLiteral("Speaker A")},
                {QStringLiteral("language"), QStringLiteral("vi")}
            }
        }}
    };
    const QVariantList runtimeSchema{QVariantMap{
        {QStringLiteral("id"), QStringLiteral("voice")},
        {QStringLiteral("type"), QStringLiteral("choice")},
        {QStringLiteral("choices"), QVariantList{QVariantMap{
            {QStringLiteral("value"), QStringLiteral("speaker_a")},
            {QStringLiteral("text"), QStringLiteral("speaker_a")}
        }}}
    }};

    const QVariantList merged = CapabilitySettingsSchema::merge(
        familyConfig, QStringLiteral("tts"), runtimeSchema);
    QCOMPARE(merged.size(), 1);
    const QVariantMap voice = merged.first().toMap();
    const QVariantList choices = voice.value(QStringLiteral("choices")).toList();
    QCOMPARE(choices.size(), 1);
    QCOMPARE(choices.first().toMap().value(QStringLiteral("value")).toString(),
             QStringLiteral("speaker_a"));
    QCOMPARE(choices.first().toMap().value(QStringLiteral("text")).toString(),
             QStringLiteral("Speaker A"));
    QCOMPARE(choices.first().toMap().value(QStringLiteral("detail")).toString(),
             QStringLiteral("vi"));
}

void TestModelsAndRuntimes::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
}

void TestModelsAndRuntimes::testLlamaContextTranslationParser()
{
    QStringList translations;
    QVERIFY(LlamaTranslationInterface::parseContextTranslation(
        QStringLiteral("[[LA_SEG_000001]]\nỞ vị trí thứ năm là Chiến tranh Việt Nam.\n\n"
                       "[[LA_SEG_000002]]\nỞ vị trí thứ tư là Chiến tranh Afghanistan."),
        2, &translations));
    QCOMPARE(translations, QStringList({
        QStringLiteral("Ở vị trí thứ năm là Chiến tranh Việt Nam."),
        QStringLiteral("Ở vị trí thứ tư là Chiến tranh Afghanistan.")}));

    QVERIFY(!LlamaTranslationInterface::parseContextTranslation(
        QStringLiteral("[[LA_SEG_000001]]\nMột\n[[LA_SEG_000003]]\nBa"),
        2, &translations));
    QVERIFY(translations.isEmpty());
    QVERIFY(!LlamaTranslationInterface::parseContextTranslation(
        QStringLiteral("[[LA_SEG_000001]]\nMột"), 2, &translations));
}

void TestModelsAndRuntimes::testOptionalLlamaTranslationRuntimeLoad()
{
    const QString runtimePath =
        qEnvironmentVariable("LASTUDIO_TEST_LLAMA_RUNTIME_PATH").trimmed();
    const QString modelPath =
        qEnvironmentVariable("LASTUDIO_TEST_LLAMA_MODEL_PATH").trimmed();
    if (runtimePath.isEmpty() || modelPath.isEmpty()) {
        QSKIP("Set LASTUDIO_TEST_LLAMA_RUNTIME_PATH and LASTUDIO_TEST_LLAMA_MODEL_PATH "
              "to run the local llama.cpp model-load smoke test.");
    }

    LlamaTranslationInterface translator;
    QString error;
    QVERIFY2(translator.load(runtimePath, modelPath, &error), qPrintable(error));
    QVERIFY(translator.isLoaded());

    const QStringList sourceTexts{
        QStringLiteral("that it almost lasted 1,000 years"),
        QStringLiteral("at No. 5 it's the Vietnam War"),
        QStringLiteral("we have the Afghanistan war between the USA and Afghanistan")
    };
    const QStringList results = translator.translateBatch(
        sourceTexts,
        QStringLiteral("en"),
        QStringLiteral("vi"),
        128,
        {},
        &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(results.size(), sourceTexts.size());
    for (qsizetype i = 0; i < results.size(); ++i) {
        QVERIFY2(!results.at(i).trimmed().isEmpty(),
                 "The llama.cpp translation smoke test returned empty output.");
        QVERIFY2(!results.at(i).contains(QChar::ReplacementCharacter),
                 qPrintable(QStringLiteral("Translation contains invalid UTF-8 replacement characters: %1")
                                .arg(results.at(i))));
        QVERIFY2(results.at(i).compare(sourceTexts.at(i), Qt::CaseInsensitive) != 0,
                 qPrintable(QStringLiteral("Hy-MT2 copied the source instead of translating it: %1")
                                .arg(results.at(i))));
    }

    const QString difficultSource = QStringLiteral(
        "The war at number one was so long that it almost lasted a thousand years at number five.");
    const QVariantList durationSegments{
        QVariantMap{
            {QStringLiteral("sourceText"), difficultSource},
            {QStringLiteral("durationBudget"),
             QVariantMap{{QStringLiteral("minUnits"), 52},
                         {QStringLiteral("maxUnits"), 64},
                         {QStringLiteral("targetUnits"), 58}}},
            {QStringLiteral("protectedTokens"), QString()}}
    };
    const QStringList durationResults = translator.translateBatch(
        {difficultSource}, QStringLiteral("en"), QStringLiteral("vi"), 128, {}, &error,
        QStringLiteral("duration-translate"), durationSegments);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(durationResults.size(), 1);
    const QString durationResult = durationResults.constFirst();
    QVERIFY2(!durationResult.contains(QStringLiteral("bộ đếm"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("Duration instruction leaked into translation: %1")
                            .arg(durationResult)));
    QVERIFY2(!durationResult.contains(QStringLiteral("từ khóa được bảo vệ"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("Protected-token instruction leaked into translation: %1")
                            .arg(durationResult)));
    QVERIFY2(durationResult.size() <= difficultSource.size() * 3 + 80,
             qPrintable(QStringLiteral("Duration translation is implausibly long: %1")
                            .arg(durationResult)));
}

void TestModelsAndRuntimes::testLlamaCatalogIncludesAllWindowsX64Runtimes()
{
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot =
        QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot),
             "Test must run from the repository root to load catalog and schema files");

    CatalogManager catalog;
    QVariantMap hyMt2;
    for (const QVariant &familyValue : catalog.sttFamilies()) {
        const QVariantMap family = familyValue.toMap();
        if (family.value(QStringLiteral("id")).toString() == QStringLiteral("hy-mt2-1.8b")) {
            hyMt2 = family;
            break;
        }
    }
    QVERIFY2(!hyMt2.isEmpty(), "Hy-MT2 should be present in the catalog");
    QVERIFY2(hyMt2.value(QStringLiteral("isLastudioPick")).toBool(),
             "Hy-MT2 should be marked as an LA Studio Pick");

    const QSet<QString> expectedRuntimeIds{
        QStringLiteral("llama-win-x86_64-cpu"),
        QStringLiteral("llama-win-x86_64-cuda-12.4"),
        QStringLiteral("llama-win-x86_64-cuda-13.3"),
        QStringLiteral("llama-win-x86_64-vulkan"),
        QStringLiteral("llama-win-x86_64-hip-radeon"),
        QStringLiteral("llama-win-x86_64-sycl"),
        QStringLiteral("llama-win-x86_64-openvino")
    };
    QSet<QString> foundRuntimeIds;
    for (const QVariant &runtimeValue : hyMt2.value(QStringLiteral("runtimes")).toList()) {
        const QVariantMap runtime = runtimeValue.toMap();
        const QString runtimeId = runtime.value(QStringLiteral("id")).toString();
        foundRuntimeIds.insert(runtimeId);
        QCOMPARE(runtime.value(QStringLiteral("engineFamily")).toString(), QStringLiteral("llama"));
        QCOMPARE(runtime.value(QStringLiteral("backend")).toString(), QStringLiteral("llama"));
        QCOMPARE(runtime.value(QStringLiteral("version")).toString(), QStringLiteral("b10036"));
        QCOMPARE(runtime.value(QStringLiteral("kind")).toString(), QStringLiteral("dynamic-library"));
        QCOMPARE(runtime.value(QStringLiteral("library")).toString(), QStringLiteral("llama.dll"));
        QCOMPARE(runtime.value(QStringLiteral("protocolVersion")).toString(),
                 QStringLiteral("llama-c-api-b10036"));
        QCOMPARE(runtime.value(QStringLiteral("sha256")).toString().size(), 64);

        if (runtimeId.contains(QStringLiteral("cuda"))) {
            const QVariantList dependencies =
                runtime.value(QStringLiteral("dependencyDownloads")).toList();
            QCOMPARE(dependencies.size(), 1);
            const QVariantMap dependency = dependencies.first().toMap();
            QCOMPARE(dependency.value(QStringLiteral("dependency")).toString(),
                     QStringLiteral("cuda-runtime"));
            QCOMPARE(dependency.value(QStringLiteral("sha256")).toString().size(), 64);
            QVERIFY(dependency.value(QStringLiteral("url")).toString()
                        .contains(QStringLiteral("/releases/download/b10036/")));
        }
    }
    QCOMPARE(foundRuntimeIds, expectedRuntimeIds);
}

void TestModelsAndRuntimes::testStudioSelectionRepositoryRemembersFilesPerFamily()
{
    const QString connectionName =
        QStringLiteral("selection-test-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(database.open());

        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE active_capability_selections ("
            "capability_id TEXT PRIMARY KEY, family_id TEXT NOT NULL, runtime_id TEXT, "
            "runtime_version TEXT, selected_files_json TEXT NOT NULL DEFAULT '{}', "
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE model_family_file_selections ("
            "capability_id TEXT NOT NULL, family_id TEXT NOT NULL, "
            "selected_files_json TEXT NOT NULL DEFAULT '{}', "
            "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
            "PRIMARY KEY (capability_id, family_id))")));

        StudioSelectionRepository repository(connectionName);
        StudioConfiguration first;
        first.capabilityId = QStringLiteral("translation");
        first.familyId = QStringLiteral("m2m100-418m");
        first.selectedFiles.insert(QStringLiteral("model"), QStringLiteral("m2m-q8.gguf"));
        repository.saveActiveSelection(first);

        StudioConfiguration second;
        second.capabilityId = QStringLiteral("translation");
        second.familyId = QStringLiteral("hy-mt2-1.8b");
        second.selectedFiles.insert(QStringLiteral("model"), QStringLiteral("hy-mt2-q6.gguf"));
        repository.saveActiveSelection(second);

        QCOMPARE(repository.selectionFor(QStringLiteral("translation")).familyId,
                 QStringLiteral("hy-mt2-1.8b"));
        QCOMPARE(repository.fileSelectionForFamily(QStringLiteral("translation"),
                                                   QStringLiteral("m2m100-418m"))
                     .value(QStringLiteral("model")).toString(),
                 QStringLiteral("m2m-q8.gguf"));
        QCOMPARE(repository.fileSelectionForFamily(QStringLiteral("translation"),
                                                   QStringLiteral("hy-mt2-1.8b"))
                     .value(QStringLiteral("model")).toString(),
                 QStringLiteral("hy-mt2-q6.gguf"));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestModelsAndRuntimes::testModelManagerConcreteModelDir()
{
    qDebug() << "--- START: testModelManagerConcreteModelDir ---";
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString modelsRoot = tempDir.filePath(QStringLiteral("models"));
    ModelManager models;
    models.setModelsRoot(modelsRoot);

    QCOMPARE(QDir(models.concreteModelDir(QStringLiteral("cstr/example-GGUF"))).absolutePath(),
             QDir(modelsRoot + QStringLiteral("/cstr/example-GGUF")).absolutePath());

    QDir().mkpath(models.concreteModelDir(QStringLiteral("cstr/example-GGUF")));
    QFile directFile(QDir(models.concreteModelDir(QStringLiteral("cstr/example-GGUF")))
                         .absoluteFilePath(QStringLiteral("model.gguf")));
    QVERIFY(directFile.open(QIODevice::WriteOnly));
    directFile.write("direct");
    directFile.close();

    const QString legacyNestedDir = QDir(modelsRoot).absoluteFilePath(QStringLiteral("models/cstr/legacy-GGUF"));
    QDir().mkpath(legacyNestedDir);
    QFile legacyFile(QDir(legacyNestedDir).absoluteFilePath(QStringLiteral("legacy.gguf")));
    QVERIFY(legacyFile.open(QIODevice::WriteOnly));
    legacyFile.write("legacy");
    legacyFile.close();

    models.scanLocalModels();
    QVERIFY(models.hasFile(QStringLiteral("cstr/example-GGUF"), QStringLiteral("model.gguf")));
    QVERIFY(models.hasFile(QStringLiteral("cstr/legacy-GGUF"), QStringLiteral("legacy.gguf")));
}

void TestModelsAndRuntimes::testModelManagerResolvesSplitVirtualModelFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString modelsRoot = tempDir.filePath(QStringLiteral("models"));
    const QString codecDir = QDir(modelsRoot).absoluteFilePath(QStringLiteral("pnnbao-ump/VieNeu-Codec"));
    const QString ggufDir = QDir(modelsRoot).absoluteFilePath(QStringLiteral("pnnbao-ump/VieNeu-TTS-v2-Turbo-GGUF"));
    QVERIFY(QDir().mkpath(codecDir));
    QVERIFY(QDir().mkpath(ggufDir));

    auto writeFile = [](const QString &path, const QByteArray &data) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(data);
        return true;
    };

    QVERIFY(writeFile(QDir(codecDir).absoluteFilePath(QStringLiteral(".la-info.json")),
                      R"({"id":"vieneu-tts-v2-turbo","task":"tts"})"));
    QVERIFY(writeFile(QDir(codecDir).absoluteFilePath(QStringLiteral("vieneu_encoder.onnx")), "encoder"));
    QVERIFY(writeFile(QDir(codecDir).absoluteFilePath(QStringLiteral("vieneu_decoder.onnx")), "decoder"));
    QVERIFY(writeFile(QDir(ggufDir).absoluteFilePath(QStringLiteral(".la-info.json")),
                      R"({"id":"vieneu-tts-v2-turbo","task":"tts"})"));
    QVERIFY(writeFile(QDir(ggufDir).absoluteFilePath(QStringLiteral("vieneu-tts-v2-turbo.gguf")), "gguf"));
    QVERIFY(writeFile(QDir(ggufDir).absoluteFilePath(QStringLiteral("voices.json")), "{}"));

    ModelManager models;
    models.setModelsRoot(modelsRoot);
    models.scanLocalModels();

    QCOMPARE(QFileInfo(models.filePath(QStringLiteral("pnnbao-ump/VieNeu-TTS-v2-Turbo-GGUF"),
                                       QStringLiteral("vieneu-tts-v2-turbo.gguf"))).absoluteFilePath(),
             QFileInfo(QDir(ggufDir).absoluteFilePath(QStringLiteral("vieneu-tts-v2-turbo.gguf"))).absoluteFilePath());
    QCOMPARE(QFileInfo(models.filePath(QStringLiteral("pnnbao-ump/VieNeu-Codec"),
                                       QStringLiteral("vieneu_encoder.onnx"))).absoluteFilePath(),
             QFileInfo(QDir(codecDir).absoluteFilePath(QStringLiteral("vieneu_encoder.onnx"))).absoluteFilePath());
    QCOMPARE(
        QFileInfo(models.firstAvailableFilePath(
                      QStringLiteral("pnnbao-ump/VieNeu-TTS-v2-Turbo-GGUF"),
                      {QStringLiteral("missing-default.gguf"),
                       QStringLiteral("vieneu-tts-v2-turbo.gguf")}))
            .absoluteFilePath(),
        QFileInfo(QDir(ggufDir).absoluteFilePath(QStringLiteral("vieneu-tts-v2-turbo.gguf")))
            .absoluteFilePath());
}

void TestModelsAndRuntimes::testCapabilityFamilyModelSuitability()
{
    qDebug() << "--- START: testCapabilityFamilyModelSuitability ---";
    ModelManager models;
    Settings settings;
    RuntimeManager runtimes(nullptr, &settings);
    CapabilityFamilyModel model(&models, &runtimes, nullptr, &settings);

    QVariantMap family;
    family[QStringLiteral("file")] = QStringLiteral("default.gguf");
    family[QStringLiteral("size")] = QStringLiteral("2.0 GB");

    QVariantMap requirement;
    requirement[QStringLiteral("file")] = QStringLiteral("large.gguf");
    requirement[QStringLiteral("size")] = QStringLiteral("120.0 GB");

    // Suitability check should respect the requirement size (120 GB) rather than default family (2 GB)
    // and label it as unsuitable on typical test systems without 128GB VRAM/RAM.
    bool suitable = model.isModelSuitable(QStringLiteral("large.gguf"), family, requirement);
    QVERIFY(!suitable);
}

void TestModelsAndRuntimes::testVoiceDesignFamiliesExposeRuntimeOptions()
{
    qDebug() << "--- START: testVoiceDesignFamiliesExposeRuntimeOptions ---";
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot = QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot), "Test must run from the repository root to load catalog and schema files");

    CatalogManager catalog;
    RegistryManager registry;
    registry.initializeFromCatalog(&catalog);

    const QStringList expectedFamilyIds = {
        QStringLiteral("omnivoice"),
        QStringLiteral("qwen3-tts-1.7b-voicedesign")
    };

    for (const QString &familyId : expectedFamilyIds) {
        QVariantMap registryFamily;
        for (const QVariant &familyValue : registry.ttsFamilies()) {
            const QVariantMap family = familyValue.toMap();
            if (family.value(QStringLiteral("id")).toString() == familyId) {
                registryFamily = family;
                break;
            }
        }

        QVERIFY2(!registryFamily.isEmpty(), qPrintable(familyId + QStringLiteral(" should be available through the TTS-shared family pool")));
        QVERIFY2(!registryFamily.value(QStringLiteral("runtimes")).toList().isEmpty(),
                 qPrintable(familyId + QStringLiteral(" should keep its catalog runtime definitions")));
    }

    ModelManager models;
    Settings settings;
    RuntimeManager runtimes(&catalog, &settings);
    CapabilityFamilyModel familyModel(&models, &runtimes, &registry, &settings);
    familyModel.setCapability(QStringLiteral("voice-design"));

    for (const QString &familyId : expectedFamilyIds) {
        const QVariantMap modelItem = familyModel.itemForFamily(familyId);
        QVERIFY2(!modelItem.isEmpty(), qPrintable(QStringLiteral("CapabilityFamilyModel should expose ") + familyId));
        const QVariantList runtimeOptions = modelItem.value(QStringLiteral("runtimeOptions")).toList();
        QVERIFY2(!runtimeOptions.isEmpty(),
                 qPrintable(familyId + QStringLiteral(" should have runtime options to render for user selection")));

        QSet<QString> runtimeIds;
        for (const QVariant &runtimeValue : runtimeOptions) {
            const QString runtimeId = runtimeValue.toMap().value(QStringLiteral("id")).toString();
            QVERIFY2(!runtimeIds.contains(runtimeId),
                     qPrintable(familyId + QStringLiteral(" should expose one runtime row per runtime id")));
            runtimeIds.insert(runtimeId);
        }
    }
}

void TestModelsAndRuntimes::testTranslationRecommendationUsesCompatibleRuntime()
{
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot = QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot), "Test must run from the repository root to load catalog data");

    CatalogManager catalog;
    RegistryManager registry;
    registry.initializeFromCatalog(&catalog);
    ModelManager models;
    Settings settings;
    RuntimeManager runtimes(&catalog, &settings);
    CapabilityFamilyModel familyModel(&models, &runtimes, &registry, &settings);
    familyModel.setCapability(QStringLiteral("translation"));

    const QVariantMap recommendation = familyModel.recommendedConfiguration();
    QVERIFY2(!recommendation.isEmpty(), "Translation should expose a hardware-compatible default configuration");
    QVERIFY(!recommendation.value(QStringLiteral("familyId")).toString().isEmpty());
    QVERIFY(!recommendation.value(QStringLiteral("runtimeId")).toString().isEmpty());
    QVERIFY(!recommendation.value(QStringLiteral("reason")).toString().isEmpty());

    const QVariantMap hyMt2Default = familyModel.configurationForFamily(
        QStringLiteral("hy-mt2-1.8b"));
    QVERIFY2(!hyMt2Default.isEmpty(),
             "Automatic dubbing must be able to select Tencent Hy-MT2 explicitly");
    QCOMPARE(hyMt2Default.value(QStringLiteral("familyId")).toString(),
             QStringLiteral("hy-mt2-1.8b"));
    QVERIFY(!hyMt2Default.value(QStringLiteral("runtimeId")).toString().isEmpty());
    QVERIFY(familyModel.configurationForFamily(QStringLiteral("stale-family-id")).isEmpty());

    const QVariantMap family = familyModel.itemForFamily(
        recommendation.value(QStringLiteral("familyId")).toString());
    QVERIFY(!family.isEmpty());
    bool foundCompatibleRuntime = false;
    for (const QVariant &runtimeValue : family.value(QStringLiteral("runtimeOptions")).toList()) {
        const QVariantMap runtime = runtimeValue.toMap();
        if (runtime.value(QStringLiteral("id")).toString()
            == recommendation.value(QStringLiteral("runtimeId")).toString()) {
            foundCompatibleRuntime = runtime.value(QStringLiteral("compatible")).toBool();
            break;
        }
    }
    QVERIFY2(foundCompatibleRuntime, "Recommended translation runtime must be compatible with detected hardware");
}

void TestModelsAndRuntimes::testForcedAlignmentCatalogEntry()
{
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot = QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot), "Test must run from the repository root to load catalog and schema files");

    CatalogManager catalog;
    RegistryManager registry;
    registry.initializeFromCatalog(&catalog);

    QVariantMap category;
    for (const QVariant &categoryValue : registry.modelCategories()) {
        const QVariantMap candidate = categoryValue.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == QStringLiteral("alignment")) {
            category = candidate;
            break;
        }
    }
    QVERIFY2(!category.isEmpty(), "Alignment category should be present in the model catalog");
    QVERIFY(category.value(QStringLiteral("capabilities")).toList().contains(QStringLiteral("forced-alignment")));

    QVariantMap family;
    for (const QVariant &familyValue : registry.sttFamilies()) {
        const QVariantMap candidate = familyValue.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == QStringLiteral("mms-forced-aligner-onnx")) {
            family = candidate;
            break;
        }
    }
    QVERIFY2(!family.isEmpty(), "MMS forced aligner should be exposed through the STT family pool");
    QVERIFY(family.value(QStringLiteral("capabilities")).toList().contains(QStringLiteral("forced-alignment")));
    QVERIFY(!family.value(QStringLiteral("capabilities")).toList().contains(QStringLiteral("stt")));

    const QVariantList requirements = family.value(QStringLiteral("requiredFiles")).toList();
    QSet<QString> requiredFiles;
    for (const QVariant &requirementValue : requirements) {
        const QVariantMap requirement = requirementValue.toMap();
        requiredFiles.insert(requirement.value(QStringLiteral("file")).toString());
    }
    QVERIFY(requiredFiles.contains(QStringLiteral("onnx/model_int8.onnx")));
    QVERIFY(requiredFiles.contains(QStringLiteral("config.json")));
    QVERIFY(requiredFiles.contains(QStringLiteral("preprocessor_config.json")));
    QVERIFY(requiredFiles.contains(QStringLiteral("tokenizer_config.json")));
    QVERIFY(requiredFiles.contains(QStringLiteral("special_tokens_map.json")));
    QVERIFY(requiredFiles.contains(QStringLiteral("vocab.json")));

    const QVariantList runtimes = family.value(QStringLiteral("runtimes")).toList();
    QCOMPARE(runtimes.size(), 2);
    bool foundEnabledCpuRuntime = false;
    bool foundDisabledCudaRuntime = false;
    for (const QVariant &runtimeValue : runtimes) {
        const QVariantMap runtime = runtimeValue.toMap();
        QCOMPARE(runtime.value(QStringLiteral("engineFamily")).toString(), QStringLiteral("mms-aligner"));
        QCOMPARE(runtime.value(QStringLiteral("type")).toString(), QStringLiteral("alignment"));
        QCOMPARE(runtime.value(QStringLiteral("kind")).toString(), QStringLiteral("process"));
        QCOMPARE(runtime.value(QStringLiteral("entrypoint")).toString(), QStringLiteral("mms-aligner.exe"));
        QCOMPARE(runtime.value(QStringLiteral("protocolVersion")).toString(), QStringLiteral("1"));
        if (runtime.value(QStringLiteral("id")).toString() == QStringLiteral("mms-aligner-win-x86_64-cpu")) {
            foundEnabledCpuRuntime = true;
            QVERIFY2(runtime.value(QStringLiteral("disabledReason")).toString().isEmpty(),
                     "Published CPU alignment runtime should be enabled");
            QCOMPARE(runtime.value(QStringLiteral("sha256")).toString(),
                     QStringLiteral("0e9f7e233e58087eec890640d66977b8703f60721d13cb527894a612d2f8beff"));
            QCOMPARE(runtime.value(QStringLiteral("size")).toLongLong(), 36794521LL);
        } else if (runtime.value(QStringLiteral("id")).toString() == QStringLiteral("mms-aligner-win-x86_64-cuda-12")) {
            foundDisabledCudaRuntime = true;
            QVERIFY2(!runtime.value(QStringLiteral("disabledReason")).toString().isEmpty(),
                     "CUDA alignment runtime should remain disabled until a CUDA package is published");
        }
    }
    QVERIFY(foundEnabledCpuRuntime);
    QVERIFY(foundDisabledCudaRuntime);

    ModelManager models;
    Settings settings;
    RuntimeManager runtimesManager(&catalog, &settings);
    CapabilityFamilyModel familyModel(&models, &runtimesManager, &registry, &settings);
    familyModel.setCapability(QStringLiteral("forced-alignment"));

    const QVariantMap modelItem = familyModel.itemForFamily(QStringLiteral("mms-forced-aligner-onnx"));
    QVERIFY2(!modelItem.isEmpty(), "CapabilityFamilyModel should expose forced-alignment families");
    QCOMPARE(modelItem.value(QStringLiteral("modelId")).toString(),
             QStringLiteral("onnx-community/mms-300m-1130-forced-aligner-ONNX"));
    QCOMPARE(modelItem.value(QStringLiteral("familyCapability")).toString(), QStringLiteral("forced-alignment"));
    QCOMPARE(modelItem.value(QStringLiteral("statusKind")).toString(), QStringLiteral("setup"));
    QVERIFY(!modelItem.value(QStringLiteral("ready")).toBool());
}

void TestModelsAndRuntimes::testVoiceIsolationRuntimeCatalogEntry()
{
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot = QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot), "Test must run from the repository root to load catalog and schema files");

    CatalogManager catalog;
    RegistryManager registry;
    registry.initializeFromCatalog(&catalog);

    const QSet<QString> expectedFamilyIds{
        QStringLiteral("sherpa-onnx-uvr-vocals-ft"),
        QStringLiteral("sherpa-onnx-spleeter-2stems-fp16")
    };
    QSet<QString> foundFamilyIds;
    for (const QVariant &familyValue : registry.sttFamilies()) {
        const QVariantMap candidate = familyValue.toMap();
        const QString familyId = candidate.value(QStringLiteral("id")).toString();
        if (!expectedFamilyIds.contains(familyId)) continue;
        foundFamilyIds.insert(familyId);

        const QVariantList runtimes = candidate.value(QStringLiteral("runtimes")).toList();
        QVERIFY2(!runtimes.isEmpty(), qPrintable(familyId + QStringLiteral(" should define a runtime")));
        const QVariantMap runtime = runtimes.first().toMap();
        QCOMPARE(runtime.value(QStringLiteral("id")).toString(), QStringLiteral("sherpa-onnx-win-x86_64-cpu"));
        const QString source = runtime.value(QStringLiteral("source")).toString();
        const QString version = runtime.value(QStringLiteral("version")).toString();
        const QString asset = runtime.value(QStringLiteral("asset")).toString();
        QCOMPARE(source, QStringLiteral("https://github.com/k2-fsa/sherpa-onnx/releases/download/"));
        QCOMPARE(version, QStringLiteral("v1.13.4"));
        QCOMPARE(asset, QStringLiteral("sherpa-onnx-v1.13.4-win-x64-shared-MT-Release.tar.bz2"));
        QCOMPARE(runtime.value(QStringLiteral("engineFamily")).toString(), QStringLiteral("sherpa-onnx"));
        QCOMPARE(runtime.value(QStringLiteral("name")).toString(), QStringLiteral("sherpa-onnx (x64)"));
        QCOMPARE(runtime.value(QStringLiteral("library")).toString(), QStringLiteral("lib/sherpa-onnx-c-api.dll"));
        QVERIFY((source + version + QStringLiteral("/") + asset).contains(QStringLiteral("/releases/download/")));
    }
    QCOMPARE(foundFamilyIds, expectedFamilyIds);
}

void TestModelsAndRuntimes::testProcessRuntimeManifest()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile executable(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("mms-aligner.exe")));
    QVERIFY(executable.open(QIODevice::WriteOnly));
    executable.write("runtime");
    executable.close();

    QJsonObject manifest{
        {QStringLiteral("id"), QStringLiteral("mms-aligner-win-x86_64-cpu")},
        {QStringLiteral("engineFamily"), QStringLiteral("mms-aligner")},
        {QStringLiteral("variant"), QStringLiteral("win-x86_64-cpu")},
        {QStringLiteral("version"), QStringLiteral("v0.1.0")},
        {QStringLiteral("type"), QStringLiteral("alignment")},
        {QStringLiteral("kind"), QStringLiteral("process")},
        {QStringLiteral("entrypoint"), QStringLiteral("mms-aligner.exe")},
        {QStringLiteral("protocolVersion"), QStringLiteral("1")},
        {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("forced-alignment")}},
        {QStringLiteral("modelFormats"), QJsonArray{QStringLiteral("onnx")}}
    };
    QFile manifestFile(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("backend-manifest.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly));
    manifestFile.write(QJsonDocument(manifest).toJson());
    manifestFile.close();

    Settings settings;
    RuntimeManager manager(nullptr, &settings);
    const RuntimeInfo info = manager.inspectRuntimeDirectory(tempDir.path());
    QCOMPARE(info.kind, QStringLiteral("process"));
    QCOMPARE(info.executablePath, executable.fileName());
    QVERIFY(info.libraryPath.isEmpty());
    QCOMPARE(info.protocolVersion, QStringLiteral("1"));
    QVERIFY(info.capabilities.contains(QStringLiteral("forced-alignment")));
    QVERIFY(info.isUsable());
}

void TestModelsAndRuntimes::testProcessRuntimeRejectsMissingEntrypoint()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QJsonObject manifest{
        {QStringLiteral("id"), QStringLiteral("mms-aligner-win-x86_64-cpu")},
        {QStringLiteral("engineFamily"), QStringLiteral("mms-aligner")},
        {QStringLiteral("variant"), QStringLiteral("win-x86_64-cpu")},
        {QStringLiteral("version"), QStringLiteral("v0.1.0")},
        {QStringLiteral("kind"), QStringLiteral("process")},
        {QStringLiteral("entrypoint"), QStringLiteral("missing.exe")}
    };
    QFile manifestFile(QDir(tempDir.path()).absoluteFilePath(QStringLiteral("backend-manifest.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly));
    manifestFile.write(QJsonDocument(manifest).toJson());
    manifestFile.close();

    Settings settings;
    RuntimeManager manager(nullptr, &settings);
    QVERIFY(!manager.inspectRuntimeDirectory(tempDir.path()).isUsable());
}

void TestModelsAndRuntimes::testVieNeuV3CatalogIncludesMossExternalData()
{
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot = QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot), "Test must run from the repository root to load catalog and schema files");

    CatalogManager catalog;
    QVariantMap vieneuV3;
    for (const QVariant &familyValue : catalog.ttsFamilies()) {
        const QVariantMap family = familyValue.toMap();
        if (family.value(QStringLiteral("id")).toString() == QStringLiteral("vieneu-tts-v3-turbo")) {
            vieneuV3 = family;
            break;
        }
    }

    QVERIFY2(!vieneuV3.isEmpty(), "VieNeu-TTS v3 Turbo should be present in the catalog");
    QCOMPARE(vieneuV3.value(QStringLiteral("modelId")).toString(),
             QStringLiteral("lastudio-community/VieNeu-TTS-v3-Turbo-CPP"));

    QSet<QString> requiredFiles;
    QMap<QString, QVariantMap> requirementsByFile;
    for (const QVariant &reqValue : vieneuV3.value(QStringLiteral("requiredFiles")).toList()) {
        const QVariantMap req = reqValue.toMap();
        const QString file = req.value(QStringLiteral("file")).toString();
        requiredFiles.insert(file);
        requirementsByFile.insert(file, req);
    }

    QVERIFY2(requiredFiles.contains(QStringLiteral("backbone.gguf")),
             "VieNeu-TTS v3 native pipeline must require the GGUF semantic backbone");
    QVERIFY2(requiredFiles.contains(QStringLiteral("vieneu_v3_heads.npz")),
             "VieNeu-TTS v3 native pipeline must require native heads at the model root");
    QVERIFY2(requiredFiles.contains(QStringLiteral("acoustic/vieneu_acoustic_weights.npz")),
             "VieNeu-TTS v3 native pipeline must require acoustic native weights");
    QVERIFY2(requiredFiles.contains(QStringLiteral("speaker_encoder.onnx")),
             "VieNeu-TTS v3 native pipeline must require the speaker encoder used by runtime v0.1.3");
    QVERIFY2(requiredFiles.contains(QStringLiteral("voices_v3_turbo.json")),
             "VieNeu-TTS v3 native pipeline should install the published preset voice definitions");
    QVERIFY2(!requiredFiles.contains(QStringLiteral("onnx/vieneu_prefill.onnx")),
             "VieNeu-TTS v3 native catalog should not require the ONNX prefill graph");
    QVERIFY2(!requiredFiles.contains(QStringLiteral("onnx/vieneu_decode_step.onnx")),
             "VieNeu-TTS v3 native catalog should not require the ONNX decode-step graph");
    QVERIFY2(!requiredFiles.contains(QStringLiteral("onnx/vieneu_backbone_shared.data")),
             "VieNeu-TTS v3 native catalog should not require ONNX external backbone data");
    QVERIFY2(requiredFiles.contains(QStringLiteral("codec/moss_audio_tokenizer_decode_shared.data")),
             "MOSS decoder ONNX external data must be installed with the decoder graph");
    QVERIFY2(requiredFiles.contains(QStringLiteral("codec/moss_audio_tokenizer_encode.data")),
             "MOSS encoder ONNX external data must be installed with the encoder graph");

    const QVariantMap voiceCloningStudio =
        vieneuV3.value(QStringLiteral("studio")).toMap().value(QStringLiteral("voice-cloning")).toMap();
    const QVariantMap cloneDefaults = voiceCloningStudio.value(QStringLiteral("parameterDefaults")).toMap();
    QCOMPARE(cloneDefaults.value(QStringLiteral("temperature")).toDouble(), 0.8);
    QCOMPARE(cloneDefaults.value(QStringLiteral("top_k")).toInt(), 25);
    QCOMPARE(cloneDefaults.value(QStringLiteral("top_p")).toDouble(), 0.95);
    QCOMPARE(cloneDefaults.value(QStringLiteral("max_new_frames")).toInt(), 180);

    const QString cppRepo = QStringLiteral("lastudio-community/VieNeu-TTS-v3-Turbo-CPP");
    for (const QString &file : requiredFiles) {
        const QVariantMap req = requirementsByFile.value(file);
        const QString reqModelId = req.value(QStringLiteral("modelId")).toString();
        QVERIFY2(reqModelId.isEmpty() || reqModelId == cppRepo,
                 qPrintable(QStringLiteral("VieNeu-TTS v3 native file %1 must download from the CPP-ready repo").arg(file)));
    }

    const QVariantList runtimes = vieneuV3.value(QStringLiteral("runtimes")).toList();
    QCOMPARE(runtimes.size(), 1);
    for (const QVariant &runtimeValue : runtimes) {
        const QVariantMap runtime = runtimeValue.toMap();
        QCOMPARE(runtime.value(QStringLiteral("id")).toString(), QStringLiteral("vieneu-tts-win-x86_64-cpu"));
        QCOMPARE(runtime.value(QStringLiteral("label")).toString(), QStringLiteral("CPU"));
        QCOMPARE(runtime.value(QStringLiteral("asset")).toString(), QStringLiteral("vieneu-tts-win-cpu.zip"));
        QCOMPARE(runtime.value(QStringLiteral("engineFamily")).toString(), QStringLiteral("vieneu-tts"));
        QCOMPARE(runtime.value(QStringLiteral("backend")).toString(), QStringLiteral("vieneu-tts"));
        QCOMPARE(runtime.value(QStringLiteral("library")).toString(), QStringLiteral("vieneu-tts.dll"));
        QCOMPARE(runtime.value(QStringLiteral("version")).toString(), QStringLiteral("v0.1.3"));
        QCOMPARE(runtime.value(QStringLiteral("pipelineProfile")).toString(), QStringLiteral("vieneu-v3-native"));
    }
}

void TestModelsAndRuntimes::testCapabilityFamilyModelAcceptsExistingModelFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString modelId = QStringLiteral("lastudio-community/VieNeu-TTS-v3-Turbo-CPP");
    const QString fileName = QStringLiteral("vieneu_v3_heads.npz");
    ModelManager models;
    models.setModelsRoot(tempDir.path());

    const QString modelDir = models.concreteModelDir(modelId);
    QVERIFY(QDir().mkpath(modelDir));
    QFile headsFile(QDir(modelDir).absoluteFilePath(fileName));
    QVERIFY(headsFile.open(QIODevice::WriteOnly));
    headsFile.write("partial");
    headsFile.close();

    models.scanLocalModels();

    CapabilityFamilyModel familyModel(&models, nullptr, nullptr, nullptr);
    QVariantMap family;
    family.insert(QStringLiteral("modelId"), modelId);
    family.insert(QStringLiteral("localDir"), modelId);

    QVariantMap requirement;
    requirement.insert(QStringLiteral("file"), fileName);
    requirement.insert(QStringLiteral("size"), QStringLiteral("25 MB"));

    QVERIFY2(familyModel.isFileInstalled(family, fileName, requirement),
             "Installed detection should not reject model files based on stale catalog sizes");

    QVERIFY(headsFile.open(QIODevice::WriteOnly));
    QVERIFY(headsFile.resize(20 * 1024 * 1024));
    headsFile.close();

    QVERIFY2(familyModel.isFileInstalled(family, fileName, requirement),
             "Runtime-normalized NPZ files can be smaller than the original stored archive");

}

void TestModelsAndRuntimes::testQwen3TtsUsesAutomaticFrameLimit()
{
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot = QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot), "Test must run from the repository root to load catalog and schema files");

    CatalogManager catalog;
    QSet<QString> qwen3FamilyIds = {
        QStringLiteral("qwen3-tts-0.6b-base"),
        QStringLiteral("qwen3-tts-1.7b-base"),
        QStringLiteral("qwen3-tts-1.7b-customvoice"),
        QStringLiteral("qwen3-tts-1.7b-voicedesign")
    };

    for (const QVariant &familyValue : catalog.ttsFamilies()) {
        const QVariantMap family = familyValue.toMap();
        const QString familyId = family.value(QStringLiteral("id")).toString();
        if (!qwen3FamilyIds.contains(familyId)) {
            continue;
        }

        const QVariantMap parameter = family.value(QStringLiteral("parameterDefinitions")).toMap()
                                         .value(QStringLiteral("max_codec_steps")).toMap();
        QVERIFY2(!parameter.isEmpty(), qPrintable(familyId + QStringLiteral(" should expose max_codec_steps")));
        QCOMPARE(parameter.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(parameter.value(QStringLiteral("default")).toInt(), 0);
        qwen3FamilyIds.remove(familyId);
    }

    QVERIFY2(qwen3FamilyIds.isEmpty(), "Every Qwen3-TTS family should be present in the catalog");
}

void TestModelsAndRuntimes::testQwen3TtsDoesNotExposeUnsupportedLengthScale()
{
    const QString oldCurrentPath = QDir::currentPath();
    const auto restoreCurrentPath = qScopeGuard([oldCurrentPath]() {
        QDir::setCurrent(oldCurrentPath);
    });
    const QString repoRoot = QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath() + QStringLiteral("/..")).absolutePath();
    QVERIFY2(QDir::setCurrent(repoRoot), "Test must run from the repository root to load catalog and schema files");

    CatalogManager catalog;
    QSet<QString> qwen3FamilyIds = {
        QStringLiteral("qwen3-tts-0.6b-base"),
        QStringLiteral("qwen3-tts-1.7b-base"),
        QStringLiteral("qwen3-tts-1.7b-customvoice"),
        QStringLiteral("qwen3-tts-1.7b-voicedesign")
    };

    for (const QVariant &familyValue : catalog.ttsFamilies()) {
        const QVariantMap family = familyValue.toMap();
        const QString familyId = family.value(QStringLiteral("id")).toString();
        if (!qwen3FamilyIds.contains(familyId)) {
            continue;
        }

        const QVariantMap definitions = family.value(QStringLiteral("parameterDefinitions")).toMap();
        QVERIFY2(!definitions.contains(QStringLiteral("length_scale")),
                 qPrintable(familyId + QStringLiteral(" should not expose unsupported length_scale")));

        const QVariantMap studio = family.value(QStringLiteral("studio")).toMap();
        for (const QVariant &capabilityValue : studio) {
            const QVariantMap capability = capabilityValue.toMap();
            const QVariantList parameters = capability.value(QStringLiteral("parameters")).toList();
            QVERIFY2(!parameters.contains(QStringLiteral("length_scale")),
                     qPrintable(familyId + QStringLiteral(" should not send unsupported length_scale")));
        }

        qwen3FamilyIds.remove(familyId);
    }

    QVERIFY2(qwen3FamilyIds.isEmpty(), "Every Qwen3-TTS family should be present in the catalog");
}

void TestModelsAndRuntimes::testLogViewServicePending()
{
    qDebug() << "--- START: testLogViewServicePending ---";
    // Verify that deleting LogViewService while thread pool task is pending does not crash the system.
    {
        LogViewService *service = new LogViewService();
        service->requestLoadLogs();
        delete service;
    }
    
    // Wait slightly to let any worker task run its course safely without referencing deleted service pointer
    QThreadPool::globalInstance()->waitForDone();
}

void TestModelsAndRuntimes::testStudioConfigurationResolver()
{
    qDebug() << "--- START: testStudioConfigurationResolver ---";
    StudioConfiguration config;
    config.capabilityId = QStringLiteral("stt");
    config.familyId = QStringLiteral("whisper-tiny");
    config.runtimeId = QStringLiteral("whisper.cpp");
    config.runtimeVersion = QStringLiteral("1.0");
    config.selectedFiles[QStringLiteral("model")] = QStringLiteral("whisper-tiny.gguf");

    auto resolved = StudioConfigurationResolver::resolve(config);
    QVERIFY(!resolved.isValid);
}

} // namespace LAStudio
