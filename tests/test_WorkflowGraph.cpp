#include "test_WorkflowGraph.h"

#include "workflows/WorkflowGraphRunner.h"
#include "workflows/DubbingWorkflowDefinition.h"
#include "workflows/WorkflowArtifact.h"
#include "workflows/DubbingWorkflowNodes.h"
#include "workflows/WorkflowTranscript.h"
#include "workflows/WorkflowRunJournal.h"
#include "controllers/DubbingJobRunner.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QtTest>

namespace LAStudio {

namespace {
class ImmediateExecutor final : public WorkflowNodeExecutor
{
public:
    explicit ImmediateExecutor(QObject *parent = nullptr) : WorkflowNodeExecutor(parent) {}
    void start(const QVariantMap &inputs, const QVariantMap &parameters) override
    {
        const QVariant value = parameters.value(QStringLiteral("value"), inputs.value(QStringLiteral("input")));
        emit completed(QVariantMap{{QStringLiteral("output"), value}});
    }
    void cancel() override {}
};

WorkflowNodeDefinition definition(const QString &type, WorkflowDataType input, WorkflowDataType output)
{
    WorkflowNodeDefinition result;
    result.typeId = type;
    result.title = type;
    if (input != WorkflowDataType::Any) result.inputs.append({QStringLiteral("input"), QString(), input, true, false});
    result.outputs.append({QStringLiteral("output"), QString(), output, false, false});
    result.createExecutor = [](QObject *parent) { return new ImmediateExecutor(parent); };
    return result;
}
}

void TestWorkflowGraph::rejectsCyclesAndTypeMismatch()
{
    NodeRegistry registry;
    QVERIFY(registry.registerNode(definition(QStringLiteral("text-source"), WorkflowDataType::Any, WorkflowDataType::Text)));
    QVERIFY(registry.registerNode(definition(QStringLiteral("audio-sink"), WorkflowDataType::Audio, WorkflowDataType::Any)));

    WorkflowGraph graph;
    graph.nodes = {{QStringLiteral("source"), QStringLiteral("text-source"), QString(), {}},
                   {QStringLiteral("sink"), QStringLiteral("audio-sink"), QString(), {}}};
    graph.edges = {{QStringLiteral("source"), QStringLiteral("output"), QStringLiteral("sink"), QStringLiteral("input")},
                   {QStringLiteral("sink"), QStringLiteral("output"), QStringLiteral("source"), QStringLiteral("output")}};
    const QStringList errors = WorkflowGraphRunner(&registry).validate(graph);
    QVERIFY(errors.join(QStringLiteral("\n")).contains(QStringLiteral("cycle")));
    QVERIFY(errors.join(QStringLiteral("\n")).contains(QStringLiteral("Incompatible")));
}

void TestWorkflowGraph::runsRegisteredNodesInTopologicalOrder()
{
    NodeRegistry registry;
    QVERIFY(registry.registerNode(definition(QStringLiteral("source"), WorkflowDataType::Any, WorkflowDataType::Text)));
    QVERIFY(registry.registerNode(definition(QStringLiteral("copy"), WorkflowDataType::Text, WorkflowDataType::Text)));

    WorkflowGraph graph;
    graph.nodes = {{QStringLiteral("a"), QStringLiteral("source"), QString(), {{QStringLiteral("value"), QStringLiteral("hello")}}},
                   {QStringLiteral("b"), QStringLiteral("copy"), QString(), {}}};
    graph.edges = {{QStringLiteral("a"), QStringLiteral("output"), QStringLiteral("b"), QStringLiteral("input")}};

    WorkflowGraphRunner runner(&registry);
    QSignalSpy completedSpy(&runner, &WorkflowGraphRunner::completed);
    QSignalSpy nodeRunSpy(&runner, &WorkflowGraphRunner::nodeRunStarted);
    QVERIFY(runner.run(graph));
    QCOMPARE(completedSpy.size(), 1);
    QVERIFY(!runner.running());
    QCOMPARE(runner.progress(), 100);
    const QVariantMap artifacts = completedSpy.at(0).at(0).toMap();
    QCOMPARE(artifacts.value(QStringLiteral("b.output")).toString(), QStringLiteral("hello"));
    QVERIFY(!runner.runId().isEmpty());
    QCOMPARE(nodeRunSpy.size(), 2);
    QVERIFY(!nodeRunSpy.at(0).at(1).toString().isEmpty());
}

void TestWorkflowGraph::serializesAndRestoresVersionedLinks()
{
    WorkflowGraph graph;
    graph.id = QStringLiteral("demo.workflow");
    graph.version = 3;
    graph.kind = QStringLiteral("system");
    graph.nodes.append({QStringLiteral("source"), QStringLiteral("core.input"), QStringLiteral("Input"),
                        {{QStringLiteral("value"), QStringLiteral("hello")}}, 2, {}, false});
    graph.edges.append({QStringLiteral("source"), QStringLiteral("value"), QStringLiteral("sink"), QStringLiteral("input"),
                        QStringLiteral("source-to-sink")});

    QStringList parseErrors;
    const WorkflowGraph restored = WorkflowGraph::fromJson(graph.toJson(), &parseErrors);
    QVERIFY(parseErrors.isEmpty());
    QCOMPARE(restored.id, graph.id);
    QCOMPARE(restored.version, 3);
    QCOMPARE(restored.nodes.first().typeVersion, 2);
    QCOMPARE(restored.edges.first().id, QStringLiteral("source-to-sink"));
    QVERIFY(!restored.canonicalJson().isEmpty());
    QCOMPARE(restored.canonicalJson(), graph.canonicalJson());
}

void TestWorkflowGraph::rejectsMissingPortsAndMultipleSingleInputs()
{
    NodeRegistry registry;
    QVERIFY(registry.registerNode(definition(QStringLiteral("source"), WorkflowDataType::Any, WorkflowDataType::Text)));
    QVERIFY(registry.registerNode(definition(QStringLiteral("sink"), WorkflowDataType::Text, WorkflowDataType::Text)));
    WorkflowGraph graph;
    graph.nodes = {{QStringLiteral("a"), QStringLiteral("source"), QString(), {}},
                   {QStringLiteral("b"), QStringLiteral("source"), QString(), {}},
                   {QStringLiteral("sink"), QStringLiteral("sink"), QString(), {}}};
    graph.edges = {{QStringLiteral("a"), QStringLiteral("output"), QStringLiteral("sink"), QStringLiteral("input"), QStringLiteral("one")},
                   {QStringLiteral("b"), QStringLiteral("missing"), QStringLiteral("sink"), QStringLiteral("input"), QStringLiteral("two")}};
    const QStringList errors = WorkflowGraphRunner(&registry).validate(graph);
    const QString joined = errors.join(QStringLiteral("\n"));
    QVERIFY(joined.contains(QStringLiteral("Unknown output port")));
    QVERIFY(joined.contains(QStringLiteral("multiple links")));
}

void TestWorkflowGraph::buildsCanonicalDubbingWorkflowDefinition()
{
    const WorkflowGraph graph = DubbingWorkflowDefinition::create();
    QCOMPARE(graph.id, QStringLiteral("system.dubbing.default"));
    QCOMPARE(graph.version, 1);
    QCOMPARE(graph.kind, QStringLiteral("system"));
    QCOMPARE(graph.nodes.size(), 13);
    QCOMPARE(graph.edges.size(), 14);
    QVERIFY(graph.interfaceDefinition.value(QStringLiteral("inputs")).toList().size() == 3);
    QCOMPARE(graph.policies.value(QStringLiteral("maxParallelNodes")).toInt(), 2);
    QCOMPARE(graph.nodes.at(0).id, QStringLiteral("media-input"));
    QCOMPARE(graph.nodes.at(5).typeId, QStringLiteral("text.translate-transcript"));
    QCOMPARE(graph.edges.at(0).id, QStringLiteral("l01"));
    QVERIFY(graph.topologicalOrder().size() == graph.nodes.size());
    QVERIFY(graph.canonicalJson().contains("system.dubbing.default"));
}

void TestWorkflowGraph::commitsAndResolvesAtomicArtifacts()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    WorkflowArtifactStore store(temp.path());
    QString error;
    const QString staging = store.createStagingDirectory(QStringLiteral("run-1"), QStringLiteral("node-1"), &error);
    QVERIFY2(!staging.isEmpty(), qPrintable(error));
    const QString stagedFile = QDir(staging).filePath(QStringLiteral("output.wav"));
    QFile file(stagedFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("workflow-audio") > 0);
    file.close();

    WorkflowArtifactReference reference;
    QVERIFY2(store.commitFile(stagedFile, QStringLiteral("audio.clip@1"), QStringLiteral("run-1"),
                              QStringLiteral("node-1"), reference, &error), qPrintable(error));
    QVERIFY(reference.isValid());
    QCOMPARE(reference.metadata.value(QStringLiteral("nodeRunId")).toString(), QStringLiteral("node-1"));
    QVERIFY(QFileInfo::exists(store.resolve(reference)));
    QVERIFY(!QFileInfo::exists(stagedFile));
    QVERIFY(store.resolve(WorkflowArtifactReference{QStringLiteral("bad"), QStringLiteral("audio.clip@1"), 1,
                                                    QStringLiteral("sha256:x"), QStringLiteral("../escape"), 1, {}}).isEmpty());
    const QString unsafeStaging = store.createStagingDirectory(QStringLiteral(".."), QStringLiteral(".."), &error);
    QVERIFY2(!unsafeStaging.isEmpty(), qPrintable(error));
    QVERIFY(!QDir(temp.path()).relativeFilePath(unsafeStaging).startsWith(QStringLiteral("..")));
    QVERIFY(!unsafeStaging.contains(QStringLiteral("..\\outside")));
}

void TestWorkflowGraph::validatesDubbingGraphAgainstNodeContracts()
{
    NodeRegistry registry;
    QVERIFY(registerDubbingWorkflowNodes(registry));
    const WorkflowGraph graph = DubbingWorkflowDefinition::create();
    const QStringList errors = WorkflowGraphRunner(&registry).validate(graph);
    QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QStringLiteral("\n"))));
    QVERIFY(registry.contains(QStringLiteral("text.translate-transcript"), 1));
    QCOMPARE(registry.definition(QStringLiteral("text.translate-transcript"), 1).properties.size(), 2);
}

void TestWorkflowGraph::validatesTypedTranscriptArtifacts()
{
    WorkflowTranscriptArtifact base;
    QString error;
    QVERIFY(WorkflowTranscriptArtifact::fromVariantList(
        {QVariantMap{{QStringLiteral("id"), QStringLiteral("segment-1")}, {QStringLiteral("startMs"), 0},
                     {QStringLiteral("endMs"), 1000}, {QStringLiteral("sourceText"), QStringLiteral("Hello")}}},
        base, &error));
    WorkflowTranscriptArtifact revision;
    QVERIFY(WorkflowTranscriptArtifact::mergePatches(base,
        {QVariantMap{{QStringLiteral("id"), QStringLiteral("segment-1")}, {QStringLiteral("targetText"), QStringLiteral("Xin chao")}}},
        revision, &error));
    QCOMPARE(revision.segments.first().toMap().value(QStringLiteral("sourceText")).toString(), QStringLiteral("Hello"));
    QCOMPARE(revision.segments.first().toMap().value(QStringLiteral("targetText")).toString(), QStringLiteral("Xin chao"));
    QVERIFY(!WorkflowTranscriptArtifact::fromVariantList(
        {QVariantMap{{QStringLiteral("id"), QStringLiteral("segment-1")}, {QStringLiteral("startMs"), 2}, {QStringLiteral("endMs"), 1}}},
        base, &error));
}

void TestWorkflowGraph::executesCoreDubbingNodeAdapter()
{
    DubbingJobRunner dubbingRunner(nullptr, nullptr);
    NodeRegistry registry;
    QVERIFY(registerDubbingWorkflowNodes(registry, &dubbingRunner));
    WorkflowGraph graph;
    graph.id = QStringLiteral("test.workflow");
    graph.nodes = {{QStringLiteral("input"), QStringLiteral("core.input"), QStringLiteral("Input"),
                    {{QStringLiteral("value"), QStringLiteral("source.wav")}}}};
    WorkflowGraphRunner runner(&registry);
    QSignalSpy completedSpy(&runner, &WorkflowGraphRunner::completed);
    QVERIFY(runner.run(graph));
    QCOMPARE(completedSpy.size(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toMap().value(QStringLiteral("input.value")).toString(),
             QStringLiteral("source.wav"));
}

void TestWorkflowGraph::pausesAndResumesReviewGate()
{
    DubbingJobRunner dubbingRunner(nullptr, nullptr);
    NodeRegistry registry;
    QVERIFY(registerDubbingWorkflowNodes(registry, &dubbingRunner));
    WorkflowGraph graph;
    graph.id = QStringLiteral("review.workflow");
    graph.nodes = {{QStringLiteral("review"), QStringLiteral("core.review-gate"), QString(),
                    {{QStringLiteral("mode"), QStringLiteral("always")}, {QStringLiteral("editor"), QStringLiteral("dubbing.transcript")}}}};
    WorkflowGraphRunner runner(&registry);
    QSignalSpy waitingSpy(&runner, &WorkflowGraphRunner::reviewRequested);
    QSignalSpy completedSpy(&runner, &WorkflowGraphRunner::completed);
    QVERIFY(runner.run(graph, {{QStringLiteral("review.artifact"), QStringLiteral("transcript-v1")}}));
    QVERIFY(runner.waitingForInput());
    QCOMPARE(waitingSpy.size(), 1);
    QCOMPARE(waitingSpy.at(0).at(0).toMap().value(QStringLiteral("editor")).toString(), QStringLiteral("dubbing.transcript"));
    QVERIFY(runner.resume({{QStringLiteral("action"), QStringLiteral("approve")}}));
    QCOMPARE(completedSpy.size(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toMap().value(QStringLiteral("review.artifact")).toString(), QStringLiteral("transcript-v1"));
}

void TestWorkflowGraph::persistsReviewRequestsAtomically()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    WorkflowReviewStore store(temp.path());
    WorkflowReviewRequest original;
    original.reviewId = QStringLiteral("review-1");
    original.runId = QStringLiteral("run-1");
    original.nodeRunId = QStringLiteral("node-run-1");
    original.nodeId = QStringLiteral("review-transcript");
    original.mode = QStringLiteral("always");
    original.editor = QStringLiteral("dubbing.transcript");
    original.artifact = QVariantMap{{QStringLiteral("segmentCount"), 2}};
    original.createdAt = QDateTime::currentDateTimeUtc();
    QString error;
    QVERIFY2(store.save(original, &error), qPrintable(error));
    WorkflowReviewRequest restored;
    QVERIFY2(store.load(original.reviewId, restored, &error), qPrintable(error));
    QCOMPARE(restored.runId, original.runId);
    QCOMPARE(restored.nodeId, original.nodeId);
    QCOMPARE(restored.artifact.toMap().value(QStringLiteral("segmentCount")).toInt(), 2);
    QVERIFY(store.remove(original.reviewId, &error));
    QVERIFY(!store.load(original.reviewId, restored, nullptr));
}

void TestWorkflowGraph::appendsOrderedRunJournalEvents()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    WorkflowRunJournal journal(temp.path());
    QVERIFY(journal.append(WorkflowRunEvent{0, {}, QStringLiteral("run.started"), QStringLiteral("run-1"), {},
                                           QJsonObject{{QStringLiteral("workflow"), QStringLiteral("demo")}}}));
    QVERIFY(journal.append(WorkflowRunEvent{0, {}, QStringLiteral("node.started"), QStringLiteral("run-1"), QStringLiteral("node-1"), {}}));
    const QList<WorkflowRunEvent> events = journal.read(QStringLiteral("run-1"));
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0).sequence, quint64(1));
    QCOMPARE(events.at(1).sequence, quint64(2));
    QCOMPARE(events.at(1).nodeRunId, QStringLiteral("node-1"));
}

} // namespace LAStudio
