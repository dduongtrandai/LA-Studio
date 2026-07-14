#include "SttSessionController.h"
#include "controllers/AppController.h"
#include "stt/SttEngine.h"
#include "audio/AudioRecorder.h"
#include "audio/AudioPlayer.h"
#include "controllers/HistoryService.h"
#include "core/Settings.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include "core/RegistryManager.h"
#include "StudioConfigurationResolver.h"
#include <QGuiApplication>
#include <QClipboard>
#include <algorithm>

namespace LAStudio {

SttSessionController::SttSessionController(QObject *parent)
    : QObject(parent)
{
    AppController* app = AppController::instance();
    if (app) {
        m_engine = app->stt();
        m_recorder = app->recorder();
        m_player = app->player();
        m_historyService = app->history();
        m_settings = app->settings();

        if (app->registry()) {
            m_repository = new StudioSelectionRepository(app->registry()->connectionName(), this);
        }
    }

    if (m_engine) {
        connect(m_engine, &SttEngine::progressChanged, this, &SttSessionController::progressChanged);
        connect(m_engine, &SttEngine::transcriptChanged, this, [this]() {
            m_transcript = m_engine->transcript();
            emit transcriptChanged();
        });
        connect(m_engine, &SttEngine::processingChanged, this, &SttSessionController::processingChanged);
        connect(m_engine, &SttEngine::transcriptionFinished, this, &SttSessionController::onEngineTranscriptionFinished);
    }

    if (m_recorder) {
        connect(m_recorder, &AudioRecorder::recordingChanged, this, &SttSessionController::recordingChanged);
        connect(m_recorder, &AudioRecorder::levelChanged, this, &SttSessionController::recordingLevelChanged);
        connect(m_recorder, &AudioRecorder::finished, this, &SttSessionController::onRecorderFinished);
    }

    if (m_player) {
        connect(m_player, &AudioPlayer::playingChanged, this, &SttSessionController::onPlaybackStateChanged);
        connect(m_player, &AudioPlayer::playbackFinished, this, &SttSessionController::onPlaybackStateChanged);
    }

    if (m_historyService) {
        connect(m_historyService, &HistoryService::sttHistoryChanged, this, &SttSessionController::onHistoryChanged);
    }

    if (m_settings) {
        connect(m_settings, &Settings::sttLanguageChanged, this, &SttSessionController::languageChanged);
        connect(m_settings, &Settings::sttThreadsChanged, this, &SttSessionController::threadsChanged);
        connect(m_settings, &Settings::sttTranslateChanged, this, &SttSessionController::translateChanged);
    }
}

SttSessionController::~SttSessionController() = default;

QString SttSessionController::transcript() const
{
    return m_transcript;
}

bool SttSessionController::processing() const
{
    return m_engine ? m_engine->isProcessing() : false;
}

int SttSessionController::progress() const
{
    return m_engine ? m_engine->progress() : 0;
}

bool SttSessionController::recording() const
{
    return m_recorder ? m_recorder->isRecording() : false;
}

double SttSessionController::recordingLevel() const
{
    return m_recorder ? static_cast<double>(m_recorder->level()) : 0.0;
}

QVariantList SttSessionController::history() const
{
    return m_historyService ? m_historyService->sttHistory() : QVariantList();
}

QString SttSessionController::playbackPath() const
{
    return m_playbackPath;
}

QString SttSessionController::language() const
{
    return m_settings ? m_settings->sttLanguage() : QStringLiteral("auto");
}

void SttSessionController::setLanguage(const QString &lang)
{
    if (m_settings && m_settings->sttLanguage() != lang) {
        m_settings->setSttLanguage(lang);
    }
}

int SttSessionController::threads() const
{
    return m_settings ? m_settings->sttThreads() : 0;
}

void SttSessionController::setThreads(int count)
{
    count = qBound(0, count, 64);
    if (m_settings && m_settings->sttThreads() != count) {
        m_settings->setSttThreads(count);
    }
}

bool SttSessionController::translate() const
{
    return m_settings ? m_settings->sttTranslate() : false;
}

void SttSessionController::setTranslate(bool val)
{
    if (m_settings && m_settings->sttTranslate() != val) {
        m_settings->setSttTranslate(val);
    }
}

QVariantMap SttSessionController::dynamicSettings() const
{
    return m_dynamicSettings;
}

void SttSessionController::setDynamicSettings(const QVariantMap &settings)
{
    if (m_dynamicSettings == settings) {
        return;
    }
    m_dynamicSettings = settings;
    emit dynamicSettingsChanged();
}

void SttSessionController::selectFileInput(const QString &filePathOrUrl)
{
    clearInput();

    if (filePathOrUrl.isEmpty()) return;

    m_inputPath = PathUtils::urlToLocalPath(filePathOrUrl);
    m_inputUrl = QUrl::fromLocalFile(m_inputPath);
    emit inputPathChanged();
    emit inputUrlChanged();

    m_inputLoading = true;
    emit inputLoadingChanged();

    m_activeDecoder = new SttAudioDecoder(this);
    connect(m_activeDecoder, &SttAudioDecoder::finished, this, &SttSessionController::onDecoderFinished);
    connect(m_activeDecoder, &SttAudioDecoder::errorOccurred, this, &SttSessionController::onDecoderError);

    m_activeDecoder->startDecode(m_inputPath);
}

void SttSessionController::clearInput()
{
    if (m_activeDecoder) {
        m_activeDecoder->disconnect(this);
        m_activeDecoder->deleteLater();
        m_activeDecoder = nullptr;
    }

    m_inputPath.clear();
    m_inputUrl = QUrl();
    m_inputLoading = false;
    m_inputError.clear();
    m_waveformSamples.clear();
    m_decodedSamples.clear();

    emit inputPathChanged();
    emit inputUrlChanged();
    emit inputLoadingChanged();
    emit inputErrorChanged();
    emit waveformSamplesChanged();
}

void SttSessionController::startRecording(bool systemAudio)
{
    if (m_recorder) {
        clearInput();
        m_recorder->setRecordSystemAudio(systemAudio);
        m_recorder->start();
    }
}

void SttSessionController::stopRecording()
{
    if (m_recorder) {
        m_recorder->stop();
    }
}

void SttSessionController::transcribeInput()
{
    if (!m_engine || m_decodedSamples.isEmpty()) {
        return;
    }

    m_activeJob.samples = m_decodedSamples;
    
    QString modelName = QStringLiteral("Whisper");
    if (m_repository) {
        auto selection = m_repository->selectionFor(QStringLiteral("stt"));
        auto resolved = StudioConfigurationResolver::resolve(selection);
        if (resolved.isValid) {
            modelName = resolved.family.value(QStringLiteral("title")).toString();
        }
    }
    m_activeJob.modelName = modelName;
    m_activeJob.inputOrigin = m_inputPath.isEmpty() ? QStringLiteral("Live Recording") : m_inputPath;
    m_activeJob.language = language();
    m_activeJob.threads = threads();
    m_activeJob.translate = translate();
    m_activeJob.isValid = true;

    m_engine->transcribeSamples(m_activeJob.samples, m_activeJob.language, m_activeJob.threads, m_activeJob.translate, m_dynamicSettings);
}

void SttSessionController::cancelProcessing()
{
    if (m_engine) {
        m_engine->cancelProcessing();
    }
}

void SttSessionController::clearTranscript()
{
    m_transcript.clear();
    emit transcriptChanged();
}

void SttSessionController::copyTranscript()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(transcript());
    }
}

void SttSessionController::loadHistoryItem(const QString &text, const QString &filePathOrUrl)
{
    m_transcript = text;
    emit transcriptChanged();
    selectFileInput(filePathOrUrl);
}

void SttSessionController::deleteHistoryItem(const QString &id)
{
    if (m_historyService) {
        m_historyService->deleteSttHistoryItem(id);
    }
}

void SttSessionController::clearHistory()
{
    if (m_historyService) {
        m_historyService->clearSttHistory();
    }
}

void SttSessionController::playHistoryFile(const QString &filePath)
{
    if (m_player) {
        m_player->stop();
        m_playbackPath = filePath;
        emit playbackPathChanged();
        m_player->playFile(filePath);
    }
}

void SttSessionController::stopPlayback()
{
    if (m_player) {
        m_player->stop();
    }
    m_playbackPath.clear();
    emit playbackPathChanged();
}

void SttSessionController::onDecoderFinished(const QVector<float> &samples)
{
    m_decodedSamples = samples;
    m_inputLoading = false;
    m_inputError.clear();

    updateWaveform(samples);

    emit inputLoadingChanged();
    emit inputErrorChanged();
    emit waveformSamplesChanged();

    if (m_activeDecoder) {
        m_activeDecoder->deleteLater();
        m_activeDecoder = nullptr;
    }
}

void SttSessionController::onDecoderError(const QString &error)
{
    m_inputLoading = false;
    m_inputError = error;
    m_decodedSamples.clear();
    m_waveformSamples.clear();

    emit inputLoadingChanged();
    emit inputErrorChanged();
    emit waveformSamplesChanged();

    if (m_activeDecoder) {
        m_activeDecoder->deleteLater();
        m_activeDecoder = nullptr;
    }
}

void SttSessionController::onRecorderFinished(const QByteArray &pcmData)
{
    const auto *raw = reinterpret_cast<const int16_t *>(pcmData.constData());
    int numSamples = pcmData.size() / 2;
    m_decodedSamples.resize(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        m_decodedSamples[i] = static_cast<float>(raw[i]) / 32768.0f;
    }

    updateWaveform(m_decodedSamples);
    emit waveformSamplesChanged();

    transcribeInput();
}


void SttSessionController::onEngineTranscriptionFinished(const QString &text, const QVariantList &segments)
{
    if (m_activeJob.isValid && !text.trimmed().isEmpty() && m_historyService) {
        m_historyService->addSttHistoryItem(text, m_activeJob.modelName, m_activeJob.samples);
    }
    m_activeJob.isValid = false;
    emit transcriptionFinished(text, segments);
}

void SttSessionController::onHistoryChanged()
{
    emit historyChanged();
}

void SttSessionController::onPlaybackStateChanged()
{
    if (m_player && !m_player->isPlaying()) {
        m_playbackPath.clear();
    }
    emit playbackPathChanged();
}


void SttSessionController::updateWaveform(const QVector<float> &samples)
{
    m_waveformSamples.clear();
    if (samples.isEmpty()) return;

    int step = std::max<int>(1, samples.size() / 1000);
    m_waveformSamples.reserve(samples.size() / step + 1);
    for (int i = 0; i < samples.size(); i += step) {
        m_waveformSamples.append(samples[i]);
    }
}

} // namespace LAStudio
