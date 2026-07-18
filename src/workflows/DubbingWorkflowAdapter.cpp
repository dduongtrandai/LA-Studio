#include "DubbingWorkflowAdapter.h"

#include "controllers/DubbingJobRunner.h"
#include "core/Logger.h"

namespace LAStudio {

DubbingWorkflowAdapter::DubbingWorkflowAdapter(DubbingJobRunner *runner, QObject *parent)
    : WorkflowExecutionAdapter(parent), m_runner(runner)
{
    if (!m_runner) return;
    connect(m_runner, &DubbingJobRunner::stageCompleted,
            this, &DubbingWorkflowAdapter::stageCompleted);
    connect(m_runner, &DubbingJobRunner::errorOccurred,
            this, &DubbingWorkflowAdapter::failed);
}

void DubbingWorkflowAdapter::start(const QString &nodeType, const QVariantMap &inputs,
                                   const QVariantMap &parameters)
{
    Logger::info(QStringLiteral("DubbingWorkflow"),
                 QStringLiteral("Node start type=%1 inputs=%2 parameters=%3")
                     .arg(nodeType, inputs.keys().join(QLatin1Char(',')), parameters.keys().join(QLatin1Char(','))));
    if (!m_runner) {
        emit failed(QStringLiteral("Dubbing workflow runtime is unavailable."));
        return;
    }
    if (nodeType == QStringLiteral("media.ingest")) {
        m_runner->startIngest(inputs.value(QStringLiteral("media")).toString());
    } else if (nodeType == QStringLiteral("audio.source-separate")) {
        m_runner->startSourceSeparation(inputs.value(QStringLiteral("audio")).toString(), parameters);
    } else if (nodeType == QStringLiteral("audio.transcribe")) {
        m_runner->startTranscription(parameters.value(QStringLiteral("language"), QStringLiteral("auto")).toString(),
                                     inputs.value(QStringLiteral("audio")).toString());
    } else if (nodeType == QStringLiteral("text.translate-transcript")) {
        m_runner->startTranslation(parameters.value(QStringLiteral("sourceLanguage"), QStringLiteral("auto")).toString(),
                                   parameters.value(QStringLiteral("targetLanguage")).toString(),
                                   inputs.value(QStringLiteral("transcript")).toList());
    } else if (nodeType == QStringLiteral("dubbing.synthesize-segments")) {
        m_runner->startAudioGeneration(inputs.value(QStringLiteral("transcript")).toList(),
                                       parameters.value(QStringLiteral("projectPath")).toString());
    } else if (nodeType == QStringLiteral("audio.mix-timeline")) {
        m_runner->renderPreview(inputs.value(QStringLiteral("timeline")).toList(),
                                parameters.value(QStringLiteral("projectPath")).toString(),
                                parameters.value(QStringLiteral("outputPath")).toString());
    } else if (nodeType == QStringLiteral("media.export")) {
        m_runner->startExport(inputs.value(QStringLiteral("sourceMedia")).toString(),
                              parameters.value(QStringLiteral("destination")).toString());
    } else {
        emit failed(QStringLiteral("No execution capability exists for node type: %1").arg(nodeType));
    }
}

void DubbingWorkflowAdapter::cancel()
{
    if (m_runner) m_runner->cancel();
}

void DubbingWorkflowAdapter::resume(const QVariantMap &decision)
{
    Q_UNUSED(decision)
    // Review gates are owned by the graph executor because they are workflow
    // control-flow, not a Dubbing-specific execution capability.
}

} // namespace LAStudio
