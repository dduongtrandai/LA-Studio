#pragma once

#include <QObject>
#include <QtQml/qqml.h>
#include <QVariantList>
#include <QVariantMap>

#include "subtitles/TimedTextCue.h"

namespace LAStudio {

class TtsEngine;
class AudioPlayer;
class HistoryService;
class TimedSpeechPipeline;

class SubtitleVoiceController final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SubtitleVoiceController is managed by AppController")

    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QVariantList cues READ cues NOTIFY cuesChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int currentCue READ currentCue NOTIFY progressChanged)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool ttsReady READ ttsReady NOTIFY ttsReadyChanged)
    Q_PROPERTY(int activePlaybackIndex READ activePlaybackIndex NOTIFY activePlaybackIndexChanged)

public:
    explicit SubtitleVoiceController(TtsEngine *tts, AudioPlayer *player,
                                     HistoryService *history, QObject *parent = nullptr);

    QString sourcePath() const { return m_sourcePath; }
    QVariantList cues() const { return m_cues; }
    QString phase() const { return m_phase; }
    bool processing() const { return m_processing; }
    int progress() const { return m_progress; }
    int currentCue() const { return m_currentCue; }
    QString outputPath() const { return m_outputPath; }
    QVariantMap summary() const { return m_summary; }
    QString error() const { return m_error; }
    bool ttsReady() const;
    int activePlaybackIndex() const { return m_activePlaybackIndex; }

    Q_INVOKABLE bool importSrt(const QString &path);
    Q_INVOKABLE void generate(const QVariantMap &ttsSettings);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool saveOutput(const QString &destination);
    Q_INVOKABLE void playCue(int index);
    Q_INVOKABLE void playOutput();
    Q_INVOKABLE void pausePlayback();
    Q_INVOKABLE void resumePlayback();
    Q_INVOKABLE void seekPlayback(qint64 positionMs);
    Q_INVOKABLE void stopPlayback();

signals:
    void sourcePathChanged();
    void cuesChanged();
    void phaseChanged();
    void processingChanged();
    void progressChanged();
    void outputPathChanged();
    void summaryChanged();
    void errorChanged();
    void ttsReadyChanged();
    void activePlaybackIndexChanged();

private slots:
    void onPipelineCueUpdated(int index, const QVariantMap &patch);
    void onPipelinePhaseChanged(const QString &phase);
    void onPipelineProgressChanged(int progress);
    void onPipelineError(const QString &message);
    void onPipelineFinished(const QString &outputPath, const QVariantMap &summary);

private:
    void updateCue(int index, const QVariantMap &patch);
    void setError(const QString &error);

    TtsEngine *m_tts = nullptr;
    AudioPlayer *m_player = nullptr;
    HistoryService *m_history = nullptr;
    TimedSpeechPipeline *m_pipeline = nullptr;
    QString m_sourcePath;
    QVector<TimedTextCue> m_typedCues;
    QVariantList m_cues;
    QString m_phase = QStringLiteral("idle");
    bool m_processing = false;
    int m_progress = 0;
    int m_currentCue = -1;
    QString m_outputPath;
    QVariantMap m_summary;
    QString m_error;
    QString m_historyModelName;
    QString m_historyVoiceName;
    int m_activePlaybackIndex = -1;
};

} // namespace LAStudio
