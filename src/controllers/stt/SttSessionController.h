#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QUrl>
#include <QFutureWatcher>
#include <QAtomicInteger>
#include "core/StudioSelectionRepository.h"
#include "SttAudioDecoder.h"

namespace LAStudio {

class SttEngine;
class AudioRecorder;
class AudioPlayer;
class HistoryService;
class Settings;

struct SttLongFormChunk {
    qint64 startMs = 0;
    qint64 endMs = 0;
};

struct SttProbeResult {
    qint64 durationMs = 0;
    QString error;
};

struct SttChunkPlanResult {
    qint64 durationMs = 0;
    QVector<SttLongFormChunk> chunks;
    QString normalizedPath;
    QString error;
};

struct SttRangeResult {
    QVector<float> samples;
    QString error;
};

struct SttJobSnapshot {
    QVector<float> samples;
    QString modelName;
    QString inputOrigin;
    QString language;
    int threads;
    bool translate;
    bool isValid = false;
};

class SttSessionController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString inputPath READ inputPath NOTIFY inputPathChanged)
    Q_PROPERTY(QUrl inputUrl READ inputUrl NOTIFY inputUrlChanged)
    Q_PROPERTY(bool inputLoading READ inputLoading NOTIFY inputLoadingChanged)
    Q_PROPERTY(QString inputError READ inputError NOTIFY inputErrorChanged)
    Q_PROPERTY(QVariantList waveformSamples READ waveformSamples NOTIFY waveformSamplesChanged)
    Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY segmentsChanged)
    Q_PROPERTY(qint64 inputDurationMs READ inputDurationMs NOTIFY inputDurationChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString processingStage READ processingStage NOTIFY processingStageChanged)
    Q_PROPERTY(int completedChunks READ completedChunks NOTIFY chunkProgressChanged)
    Q_PROPERTY(int totalChunks READ totalChunks NOTIFY chunkProgressChanged)
    Q_PROPERTY(bool resumable READ resumable NOTIFY resumableChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(double recordingLevel READ recordingLevel NOTIFY recordingLevelChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(QString playbackPath READ playbackPath NOTIFY playbackPathChanged)

    // Settings
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(int threads READ threads WRITE setThreads NOTIFY threadsChanged)
    Q_PROPERTY(bool translate READ translate WRITE setTranslate NOTIFY translateChanged)
    Q_PROPERTY(QVariantMap dynamicSettings READ dynamicSettings WRITE setDynamicSettings NOTIFY dynamicSettingsChanged)

public:
    explicit SttSessionController(QObject *parent = nullptr);
    ~SttSessionController() override;

    // Property Getters
    QString inputPath() const { return m_inputPath; }
    QUrl inputUrl() const { return m_inputUrl; }
    bool inputLoading() const { return m_inputLoading; }
    QString inputError() const { return m_inputError; }
    QVariantList waveformSamples() const { return m_waveformSamples; }
    QString transcript() const;
    QVariantList segments() const { return m_segments; }
    qint64 inputDurationMs() const { return m_inputDurationMs; }
    bool processing() const;
    int progress() const;
    QString processingStage() const { return m_processingStage; }
    int completedChunks() const { return m_completedChunks; }
    int totalChunks() const { return m_totalChunks; }
    bool resumable() const { return m_resumable; }
    bool canTranscribe() const;
    bool recording() const;
    double recordingLevel() const;
    QVariantList history() const;
    QString playbackPath() const;

    QString language() const;
    void setLanguage(const QString &lang);
    int threads() const;
    void setThreads(int count);
    bool translate() const;
    void setTranslate(bool val);
    QVariantMap dynamicSettings() const;
    void setDynamicSettings(const QVariantMap &settings);

    // Commands
    Q_INVOKABLE void selectFileInput(const QString &filePathOrUrl);
    Q_INVOKABLE void clearInput();
    Q_INVOKABLE void startRecording(bool systemAudio = false);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void transcribeInput();
    Q_INVOKABLE void cancelProcessing();
    Q_INVOKABLE void clearTranscript();
    Q_INVOKABLE void copyTranscript();
    Q_INVOKABLE bool updateSegment(const QString &id, qint64 startMs, qint64 endMs,
                                   const QString &text);
    Q_INVOKABLE bool exportSrt(const QString &filePath);
    Q_INVOKABLE bool exportVtt(const QString &filePath);
    Q_INVOKABLE void resumeProcessing();
    Q_INVOKABLE void discardCheckpoint();
    Q_INVOKABLE void loadHistoryItem(const QString &text, const QString &filePathOrUrl);

    // History playback / actions
    Q_INVOKABLE void deleteHistoryItem(const QString &id);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void playHistoryFile(const QString &filePath);
    Q_INVOKABLE void stopPlayback();

signals:
    void inputPathChanged();
    void inputUrlChanged();
    void inputLoadingChanged();
    void inputErrorChanged();
    void waveformSamplesChanged();
    void transcriptChanged();
    void segmentsChanged();
    void inputDurationChanged();
    void processingChanged();
    void progressChanged();
    void processingStageChanged();
    void chunkProgressChanged();
    void resumableChanged();
    void recordingChanged();
    void recordingLevelChanged();
    void historyChanged();
    void playbackPathChanged();
    
    void languageChanged();
    void threadsChanged();
    void translateChanged();
    void dynamicSettingsChanged();

    // Forward the timestamped backend result so composite workflows (such as
    // Dubbing) can reuse the shared STT session without duplicating inference.
    void transcriptionFinished(const QString &text, const QVariantList &segments);
    void transcriptionFailed(const QString &message);

private slots:
    void onDecoderFinished(const QVector<float> &samples);
    void onDecoderError(const QString &error);
    
    void onRecorderFinished(const QByteArray &pcmData);
    void onEngineTranscriptionFinished(const QString &text, const QVariantList &segments);
    void onHistoryChanged();
    void onPlaybackStateChanged();
    void onProbeFinished();
    void onPlanFinished();
    void onRangeFinished();

private:
    void updateWaveform(const QVector<float> &samples);
    void startLongFormPlanning(bool resume);
    void startNextLongFormChunk();
    void finishLongForm();
    void failLongForm(const QString &message);
    void writeCheckpoint();
    bool loadCheckpoint();
    QString checkpointPath() const;
    void updateLongFormProgress();

    SttEngine* m_engine = nullptr;
    AudioRecorder* m_recorder = nullptr;
    AudioPlayer* m_player = nullptr;
    HistoryService* m_historyService = nullptr;
    Settings* m_settings = nullptr;
    StudioSelectionRepository* m_repository = nullptr;

    QString m_inputPath;
    QString m_longFormAudioPath;
    QUrl m_inputUrl;
    bool m_inputLoading = false;
    QString m_inputError;
    QVariantList m_waveformSamples;
    QVector<float> m_decodedSamples;
    QString m_transcript;
    QVariantList m_segments;
    qint64 m_inputDurationMs = 0;
    bool m_longFormMode = false;
    bool m_longFormActive = false;
    bool m_longFormCancel = false;
    QString m_processingStage;
    int m_completedChunks = 0;
    int m_totalChunks = 0;
    bool m_resumable = false;
    QVector<SttLongFormChunk> m_longFormChunks;
    int m_nextChunkIndex = 0;
    QVariantList m_longFormSegments;
    QVector<float> m_longFormChunkSamples;
    QFutureWatcher<SttProbeResult>* m_probeWatcher = nullptr;
    QFutureWatcher<SttChunkPlanResult>* m_planWatcher = nullptr;
    QFutureWatcher<SttRangeResult>* m_rangeWatcher = nullptr;

    SttJobSnapshot m_activeJob;
    SttAudioDecoder* m_activeDecoder = nullptr;
    QString m_playbackPath;
    QVariantMap m_dynamicSettings;
};

} // namespace LAStudio

