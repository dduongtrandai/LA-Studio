#pragma once

#include <QElapsedTimer>
#include <QString>

namespace LAStudio {

enum class DubbingStage {
    Idle,
    Import,
    SourceSeparation,
    Transcription,
    Alignment,
    Translation,
    Tts,
    FitTiming,
    Mix,
    Export,
    Cancelled,
    Fitted,
    Mixed,
    Error
};

class DubbingRunCoordinator final
{
public:
    bool processing() const { return m_processing; }
    DubbingStage stageId() const { return m_stageId; }
    const QString &stageName() const { return m_stage; }
    int progress() const { return m_progress; }
    const QString &lastError() const { return m_lastError; }
    const QString &runId() const { return m_runId; }
    const QString &nodeRunId() const { return m_nodeRunId; }

    void ensureRun();
    void beginNode();
    void setState(bool processing, const QString &stage, int progress);
    void setProgress(int progress) { m_progress = qBound(0, progress, 100); }
    void setError(const QString &message);
    void setLastError(const QString &message) { m_lastError = message; }
    void clearError();

    qint64 elapsedMs() const;

private:
    static DubbingStage stageFromName(const QString &stage);

    bool m_processing = false;
    DubbingStage m_stageId = DubbingStage::Idle;
    QString m_stage;
    int m_progress = 0;
    QString m_lastError;
    QString m_runId;
    QString m_nodeRunId;
    QElapsedTimer m_stageTimer;
};

} // namespace LAStudio
