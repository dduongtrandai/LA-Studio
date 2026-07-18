#include "test_AlignmentWorkflow.h"

#include "controllers/alignment/AlignmentExecutionService.h"
#include "alignment/AlignmentWorkflowResolver.h"
#include "workflows/WorkflowSession.h"
#include "core/ModelManager.h"
#include <QtTest>
#include <QTemporaryDir>

namespace LAStudio {

void TestAlignmentWorkflow::missingDependenciesAreExposedAsWorkflowNodes()
{
    AlignmentExecutionService service(nullptr, nullptr);
    QSignalSpy workflowSpy(&service, &AlignmentExecutionService::workflowChanged);

    const bool ready = service.prepareWorkflow(QVariantMap{
        {QStringLiteral("mode"), QStringLiteral("automatic")},
        {QStringLiteral("modelId"), QStringLiteral("missing-aligner")},
        {QStringLiteral("sttModel"), QVariantMap{{QStringLiteral("id"), QStringLiteral("missing-stt")}}}
    });

    QVERIFY(!ready);
    QVERIFY(!service.workflowReady());
    QCOMPARE(service.workflowNodes().size(), 5);
    QCOMPARE(workflowSpy.count(), 1);
    QCOMPARE(service.workflowNodes().at(0).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("vad"));
    QCOMPARE(service.workflowNodes().at(3).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("aligner"));
    QVERIFY(service.workflowNodes().at(0).toMap().value(QStringLiteral("providerName")).toString() == QStringLiteral("CrispASR"));
    QVERIFY(service.workflowStatusText().contains(QStringLiteral("2/5")));
}

void TestAlignmentWorkflow::resolverProducesTypedGraphAndStableSignature()
{
    AlignmentWorkflowResolver resolver(nullptr, nullptr);
    QVariantMap request{{QStringLiteral("mode"), QStringLiteral("automatic")},
                        {QStringLiteral("modelId"), QStringLiteral("aligner-a")},
                        {QStringLiteral("audioPath"), QStringLiteral("first.wav")}};
    const WorkflowResolution first = resolver.resolve(request);

    QCOMPARE(first.plan.studioId, QStringLiteral("alignment"));
    QCOMPARE(first.plan.workflowId, QStringLiteral("automatic-alignment"));
    QCOMPARE(first.plan.nodes.size(), 5);
    QCOMPARE(first.plan.edges.size(), 4);
    QCOMPARE(first.plan.resources.size(), 4);
    QVERIFY(!first.plan.signature.isEmpty());
    QVERIFY(!first.plan.isReady());
    QVERIFY(first.plan.firstBlockingNode()->kind == WorkflowNodeKind::Model);
    QVERIFY(first.plan.nodes.constFirst().providerName == QStringLiteral("CrispASR"));

    request.insert(QStringLiteral("audioPath"), QStringLiteral("another-job.wav"));
    QCOMPARE(resolver.resolve(request).plan.signature, first.plan.signature);
    request.insert(QStringLiteral("modelId"), QStringLiteral("aligner-b"));
    QVERIFY(resolver.resolve(request).plan.signature != first.plan.signature);
}

void TestAlignmentWorkflow::sessionCanBeInvalidated()
{
    AlignmentWorkflowResolver resolver(nullptr, nullptr);
    WorkflowSession session;
    QSignalSpy changedSpy(&session, &WorkflowSession::changed);

    session.prepare(resolver, QVariantMap{{QStringLiteral("mode"), QStringLiteral("automatic")}});
    QVERIFY(!session.plan().signature.isEmpty());
    session.invalidate();

    QVERIFY(session.plan().signature.isEmpty());
    QVERIFY(session.nodes().isEmpty());
    QCOMPARE(session.statusText(), QStringLiteral("Not prepared"));
    QCOMPARE(changedSpy.count(), 2);
}

void TestAlignmentWorkflow::resolverNormalizesNemotronLanguageForSttStage()
{
    AlignmentWorkflowResolver resolver(nullptr, nullptr);
    const WorkflowResolution resolution = resolver.resolve(QVariantMap{
        {QStringLiteral("mode"), QStringLiteral("automatic")},
        {QStringLiteral("language"), QStringLiteral("vie")},
        {QStringLiteral("sttModel"), QVariantMap{
            {QStringLiteral("id"), QStringLiteral("nvidia/nemotron-3.5-asr-streaming-0.6b")},
            {QStringLiteral("path"), QStringLiteral("missing.gguf")}}}
    });

    const auto payload = std::dynamic_pointer_cast<const AlignmentWorkflowPayload>(resolution.payload);
    QVERIFY(payload != nullptr);
    const QVariantMap stt = payload->executionRequest.value(QStringLiteral("sttConfiguration")).toMap();
    QCOMPARE(stt.value(QStringLiteral("backend")).toString(), QStringLiteral("nemotron"));
    QCOMPARE(stt.value(QStringLiteral("language")).toString(), QStringLiteral("vi"));
    QCOMPARE(payload->executionRequest.value(QStringLiteral("language")).toString(), QStringLiteral("vie"));
}

void TestAlignmentWorkflow::installedAnchorModelsResolveConcreteArtifactFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString q4File = QStringLiteral("nemotron-3.5-asr-streaming-0.6b-q4_k.gguf");
    const QString f16File = QStringLiteral("nemotron-3.5-asr-streaming-0.6b-f16.gguf");
    for (const QString &fileName : {q4File, f16File}) {
        QFile file(QDir(tempDir.path()).absoluteFilePath(fileName));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("model");
    }

    ModelManager models;
    models.addModel(QStringLiteral("nvidia/nemotron-3.5-asr-streaming-0.6b"),
                    QStringLiteral("stt"), QStringLiteral("gguf"), tempDir.path(),
                    {f16File, q4File}, QStringLiteral("1 GB"));
    AlignmentExecutionService service(nullptr, &models);
    const QVariantList anchors = service.installedAnchorModels();

    QVariantMap preferred;
    for (const QVariant &value : anchors) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("id")).toString()
            == QStringLiteral("nvidia/nemotron-3.5-asr-streaming-0.6b")) {
            preferred = item;
            break;
        }
    }
    QVERIFY(!preferred.isEmpty());
    QCOMPARE(preferred.value(QStringLiteral("fileName")).toString(), q4File);
    QVERIFY(QFileInfo(preferred.value(QStringLiteral("path")).toString()).isFile());
}

} // namespace LAStudio
