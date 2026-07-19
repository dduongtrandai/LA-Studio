#include "controllers/dubbing/DubbingRunCoordinator.h"

#include <QUuid>

namespace LAStudio {

void DubbingRunCoordinator::ensureRun()
{
    if (m_runId.isEmpty())
        m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void DubbingRunCoordinator::beginNode()
{
    ensureRun();
    m_nodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void DubbingRunCoordinator::setState(bool processing, const QString &stage, int progress)
{
    if (processing && (!m_processing || m_stage != stage))
        m_stageTimer.start();
    m_processing = processing;
    m_stage = stage;
    m_stageId = stageFromName(stage);
    m_progress = qBound(0, progress, 100);
}

void DubbingRunCoordinator::setError(const QString &message)
{
    m_lastError = message;
    setState(false, QStringLiteral("error"), 0);
}

void DubbingRunCoordinator::clearError()
{
    m_lastError.clear();
}

qint64 DubbingRunCoordinator::elapsedMs() const
{
    return m_stageTimer.isValid() ? m_stageTimer.elapsed() : -1;
}

DubbingStage DubbingRunCoordinator::stageFromName(const QString &stage)
{
    if (stage == QStringLiteral("import")) return DubbingStage::Import;
    if (stage == QStringLiteral("source-separate") || stage == QStringLiteral("source-separation"))
        return DubbingStage::SourceSeparation;
    if (stage == QStringLiteral("transcription")) return DubbingStage::Transcription;
    if (stage == QStringLiteral("alignment")) return DubbingStage::Alignment;
    if (stage == QStringLiteral("translation")) return DubbingStage::Translation;
    if (stage == QStringLiteral("tts")) return DubbingStage::Tts;
    if (stage == QStringLiteral("fit-timing")) return DubbingStage::FitTiming;
    if (stage == QStringLiteral("mix")) return DubbingStage::Mix;
    if (stage == QStringLiteral("export")) return DubbingStage::Export;
    if (stage == QStringLiteral("cancelled")) return DubbingStage::Cancelled;
    if (stage == QStringLiteral("fitted")) return DubbingStage::Fitted;
    if (stage == QStringLiteral("mixed")) return DubbingStage::Mixed;
    if (stage == QStringLiteral("error")) return DubbingStage::Error;
    return DubbingStage::Idle;
}

} // namespace LAStudio
