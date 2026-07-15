#include "DubbingWorkflowNodes.h"

#include "controllers/DubbingJobRunner.h"
#include "DubbingWorkflowAdapter.h"
#include "core/Logger.h"
#include <QPointer>
#include <QUuid>

namespace LAStudio {

namespace {
class CapabilityNodeExecutor final : public WorkflowNodeExecutor
{
public:
    CapabilityNodeExecutor(QString typeId, WorkflowExecutionAdapter *adapter, QObject *parent)
        : WorkflowNodeExecutor(parent), m_typeId(std::move(typeId)), m_adapter(adapter) {}

    void start(const QVariantMap &inputs, const QVariantMap &parameters) override
    {
        if (m_started) return;
        m_started = true;
        m_inputs = inputs;
        m_parameters = parameters;
        if (m_typeId == QStringLiteral("core.input")) {
            emit completed({{QStringLiteral("value"), parameters.value(QStringLiteral("value"))}});
            return;
        }
        if (m_typeId == QStringLiteral("core.review-gate")) {
            m_pendingArtifact = inputs.value(QStringLiteral("artifact"));
            const QString mode = parameters.value(QStringLiteral("mode"), QStringLiteral("always")).toString();
            if (mode == QStringLiteral("never")) {
                completeReview(m_pendingArtifact);
            } else {
                emit waitingForInput({{QStringLiteral("reviewId"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                      {QStringLiteral("nodeId"), QStringLiteral("core.review-gate")},
                                      {QStringLiteral("mode"), mode},
                                      {QStringLiteral("editor"), parameters.value(QStringLiteral("editor"))},
                                      {QStringLiteral("artifact"), m_pendingArtifact}});
            }
            return;
        }
        if (m_typeId == QStringLiteral("dubbing.assign-voices")) {
            emit completed({{QStringLiteral("transcript"), inputs.value(QStringLiteral("transcript"))}});
            return;
        }
        if (m_typeId == QStringLiteral("dubbing.fit-timing")) {
            emit completed({{QStringLiteral("timeline"), inputs.value(QStringLiteral("timeline"))}});
            return;
        }
        if (!m_adapter) {
            emit failed(QStringLiteral("Workflow execution capability is unavailable."));
            return;
        }
        connect(m_adapter, &WorkflowExecutionAdapter::stageCompleted, this,
                [this](const QString &nodeId, const QVariantMap &outputs) { onStageCompleted(nodeId, outputs); });
        connect(m_adapter, &WorkflowExecutionAdapter::failed, this, [this](const QString &message) {
            if (!m_completed) {
                m_completed = true;
                emit failed(message);
            }
        });

        m_adapter->start(m_typeId, inputs, parameters);
    }

    void cancel() override
    {
        if (m_adapter) m_adapter->cancel();
    }

    void resume(const QVariantMap &decision) override
    {
        if (m_typeId != QStringLiteral("core.review-gate") || m_completed) return;
        const QString action = decision.value(QStringLiteral("action"), QStringLiteral("approve")).toString();
        if (action == QStringLiteral("reject")) {
            m_completed = true;
            emit failed(decision.value(QStringLiteral("reason"), QStringLiteral("Review rejected by user.")).toString());
            return;
        }
        completeReview(decision.value(QStringLiteral("artifact"), m_pendingArtifact));
    }

private:
    void completeReview(const QVariant &artifact)
    {
        if (m_completed) return;
        m_completed = true;
        emit completed({{QStringLiteral("artifact"), artifact}});
    }

    void onStageCompleted(const QString &nodeId, const QVariantMap &outputs)
    {
        Logger::info(QStringLiteral("DubbingWorkflow"),
                     QStringLiteral("Node completed node=%1 outputs=%2")
                         .arg(nodeId, outputs.keys().join(QLatin1Char(','))));
        const QString expected = m_typeId == QStringLiteral("media.ingest") ? QStringLiteral("ingest")
            : m_typeId == QStringLiteral("audio.source-separate") ? QStringLiteral("source-separate")
            : m_typeId == QStringLiteral("audio.transcribe") ? QStringLiteral("transcribe")
            : m_typeId == QStringLiteral("text.translate-transcript") ? QStringLiteral("translate")
            : m_typeId == QStringLiteral("dubbing.synthesize-segments") ? QStringLiteral("synthesize")
            : m_typeId == QStringLiteral("audio.mix-timeline") ? QStringLiteral("mix")
            : m_typeId == QStringLiteral("media.export") ? QStringLiteral("export") : QString();
        if (nodeId != expected) return;
        if (m_completed) return;
        m_completed = true;
        if (m_typeId == QStringLiteral("media.ingest")) {
            emit completed({{QStringLiteral("analysisAudio"), outputs.value(QStringLiteral("analysisAudioPath"))},
                            {QStringLiteral("masterAudio"), outputs.value(QStringLiteral("masterAudioPath"))},
                            {QStringLiteral("vocals"), outputs.value(QStringLiteral("analysisAudioPath"))},
                            {QStringLiteral("background"), outputs.value(QStringLiteral("backgroundAudioPath"))}});
        } else if (m_typeId == QStringLiteral("audio.source-separate")) {
            emit completed({{QStringLiteral("vocals"), outputs.value(QStringLiteral("vocals"))},
                            {QStringLiteral("background"), outputs.value(QStringLiteral("background"))},
                            {QStringLiteral("warning"), outputs.value(QStringLiteral("warning"))}});
        } else if (m_typeId == QStringLiteral("audio.transcribe") || m_typeId == QStringLiteral("text.translate-transcript")) {
            emit completed({{QStringLiteral("transcript"), outputs.value(QStringLiteral("transcript"))}});
        } else if (m_typeId == QStringLiteral("dubbing.synthesize-segments")) {
            emit completed({{QStringLiteral("timeline"), outputs.value(QStringLiteral("timeline"))}});
        } else if (m_typeId == QStringLiteral("audio.mix-timeline")) {
            emit completed({{QStringLiteral("audio"), outputs.value(QStringLiteral("audio"))}});
        } else {
            emit completed({{QStringLiteral("media"), outputs.value(QStringLiteral("media"))}});
        }
    }

    QString m_typeId;
    QPointer<WorkflowExecutionAdapter> m_adapter;
    QVariantMap m_inputs;
    QVariantMap m_parameters;
    bool m_started = false;
    bool m_completed = false;
    QVariant m_pendingArtifact;
};

WorkflowNodeDefinition definition(const QString &typeId, const QString &title,
                                  const QList<WorkflowPort> &inputs,
                                  const QList<WorkflowPort> &outputs,
                                  const QList<WorkflowPropertyDefinition> &properties = {})
{
    WorkflowNodeDefinition result;
    result.typeId = typeId;
    result.contractVersion = 1;
    result.title = title;
    result.category = QStringLiteral("dubbing");
    result.inputs = inputs;
    result.outputs = outputs;
    result.properties = properties;
    return result;
}

WorkflowPort input(const QString &id, WorkflowDataType type, bool required = true, bool multiple = false)
{ return WorkflowPort{id, id, type, required, multiple}; }

WorkflowPort output(const QString &id, WorkflowDataType type)
{ return WorkflowPort{id, id, type, false, false}; }
}

bool registerDubbingWorkflowNodes(NodeRegistry &registry, DubbingJobRunner *runner)
{
    QList<WorkflowNodeDefinition> definitions = {
        definition(QStringLiteral("core.input"), QStringLiteral("Input"), {}, {output(QStringLiteral("value"), WorkflowDataType::Media)}),
        definition(QStringLiteral("media.ingest"), QStringLiteral("Import and Normalize"),
                  {input(QStringLiteral("media"), WorkflowDataType::Media)},
                  {output(QStringLiteral("analysisAudio"), WorkflowDataType::Audio), output(QStringLiteral("masterAudio"), WorkflowDataType::Audio)}),
        definition(QStringLiteral("audio.source-separate"), QStringLiteral("Isolate Voice"),
                  {input(QStringLiteral("audio"), WorkflowDataType::Audio), input(QStringLiteral("vocals"), WorkflowDataType::Audio, false), input(QStringLiteral("background"), WorkflowDataType::Audio, false)},
                  {output(QStringLiteral("vocals"), WorkflowDataType::Audio), output(QStringLiteral("background"), WorkflowDataType::Audio), output(QStringLiteral("warning"), WorkflowDataType::Any)}),
        definition(QStringLiteral("audio.transcribe"), QStringLiteral("Transcribe"),
                  {input(QStringLiteral("audio"), WorkflowDataType::Audio)},
                  {output(QStringLiteral("transcript"), WorkflowDataType::TimedTranscript)}),
        definition(QStringLiteral("core.review-gate"), QStringLiteral("Review Gate"),
                  {input(QStringLiteral("artifact"), WorkflowDataType::Any)},
                  {output(QStringLiteral("artifact"), WorkflowDataType::Any)}),
        definition(QStringLiteral("text.translate-transcript"), QStringLiteral("Translate Transcript"),
                  {input(QStringLiteral("transcript"), WorkflowDataType::TimedTranscript)},
                  {output(QStringLiteral("transcript"), WorkflowDataType::TimedTranscript)},
                  {WorkflowPropertyDefinition{QStringLiteral("sourceLanguage"), QStringLiteral("Source language"), QStringLiteral("auto")},
                   WorkflowPropertyDefinition{QStringLiteral("targetLanguage"), QStringLiteral("Target language"), QString(), true, true}}),
        definition(QStringLiteral("dubbing.assign-voices"), QStringLiteral("Assign Voices"),
                  {input(QStringLiteral("transcript"), WorkflowDataType::TimedTranscript)},
                  {output(QStringLiteral("transcript"), WorkflowDataType::TimedTranscript)}),
        definition(QStringLiteral("dubbing.synthesize-segments"), QStringLiteral("Synthesize Segments"),
                  {input(QStringLiteral("transcript"), WorkflowDataType::TimedTranscript)},
                  {output(QStringLiteral("timeline"), WorkflowDataType::AudioTrack)}),
        definition(QStringLiteral("dubbing.fit-timing"), QStringLiteral("Fit Timing"),
                  {input(QStringLiteral("timeline"), WorkflowDataType::AudioTrack)},
                  {output(QStringLiteral("timeline"), WorkflowDataType::AudioTrack)}),
        definition(QStringLiteral("audio.mix-timeline"), QStringLiteral("Mix Timeline"),
                  {input(QStringLiteral("timeline"), WorkflowDataType::AudioTrack), input(QStringLiteral("background"), WorkflowDataType::Audio, false)},
                  {output(QStringLiteral("audio"), WorkflowDataType::Audio)}),
        definition(QStringLiteral("media.export"), QStringLiteral("Export Media"),
                  {input(QStringLiteral("sourceMedia"), WorkflowDataType::Media), input(QStringLiteral("dubbedAudio"), WorkflowDataType::Audio)},
                  {output(QStringLiteral("media"), WorkflowDataType::Media)})
    };
    if (runner) {
        for (auto &item : definitions) {
            const QString typeId = item.typeId;
            auto *adapter = new DubbingWorkflowAdapter(runner, runner);
            item.createExecutor = [typeId, adapter](QObject *parent) {
                return new CapabilityNodeExecutor(typeId, adapter, parent);
            };
        }
    }
    bool result = true;
    for (const auto &item : definitions) result = registry.registerNode(item) && result;
    return result;
}

bool registerDubbingWorkflowNodes(NodeRegistry &registry)
{
    return registerDubbingWorkflowNodes(registry, nullptr);
}

} // namespace LAStudio
