#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QUrl>
#include "core/StudioSelectionRepository.h"
#include "SttAudioDecoder.h"

namespace LAStudio {

class SttEngine;
class AudioRecorder;
class AudioPlayer;
class HistoryService;
class Settings;

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
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
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
    bool processing() const;
    int progress() const;
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
    void processingChanged();
    void progressChanged();
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

private:
    void updateWaveform(const QVector<float> &samples);

    SttEngine* m_engine = nullptr;
    AudioRecorder* m_recorder = nullptr;
    AudioPlayer* m_player = nullptr;
    HistoryService* m_historyService = nullptr;
    Settings* m_settings = nullptr;
    StudioSelectionRepository* m_repository = nullptr;

    QString m_inputPath;
    QUrl m_inputUrl;
    bool m_inputLoading = false;
    QString m_inputError;
    QVariantList m_waveformSamples;
    QVector<float> m_decodedSamples;
    QString m_transcript;

    SttJobSnapshot m_activeJob;
    SttAudioDecoder* m_activeDecoder = nullptr;
    QString m_playbackPath;
    QVariantMap m_dynamicSettings;
};

} // namespace LAStudio

