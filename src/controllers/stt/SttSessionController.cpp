#include "SttSessionController.h"
#include "controllers/app/AppController.h"
#include "stt/SttEngine.h"
#include "audio/AudioRecorder.h"
#include "audio/AudioPlayer.h"
#include "controllers/shared/HistoryService.h"
#include "core/Settings.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include "core/RegistryManager.h"
#include "StudioConfigurationResolver.h"
#include "audio/LongFormAudioSource.h"
#include "stt/SttSubtitleService.h"
#include "runtimes/CrispAlignmentInterface.h"
#include "dubbing/AlignmentRefinementService.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace LAStudio {

namespace {

constexpr qint64 kLongFormThresholdMs = 10 * 60 * 1000;
constexpr qint64 kDefaultChunkMs = 28000;
constexpr qint64 kDefaultOverlapMs = 1000;
constexpr qint64 kVadWindowMs = 5 * 60 * 1000;

QString normalizeForCompare(const QString &value)
{
    return value.toLower().normalized(QString::NormalizationForm_KC)
        .replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), QStringLiteral(" "))
        .simplified();
}

QString findRuntimeLibrary(const QString &path)
{
    QFileInfo info(path);
    if (info.isFile()) return info.absoluteFilePath();
    if (!info.isDir()) return {};
#ifdef Q_OS_WIN
    const QString candidate = QDir(path).filePath(QStringLiteral("crispasr.dll"));
#else
    const QString candidate = QDir(path).filePath(QStringLiteral("libcrispasr.so"));
#endif
    return QFileInfo(candidate).isFile() ? candidate : QString();
}

SttChunkPlanResult buildChunkPlan(const QString &path, const QVariantMap &settings)
{
    SttChunkPlanResult result;
    QString workingPath;
    const QString cacheRoot = PathUtils::cacheDir() + QStringLiteral("/stt-long-form/audio-cache");
    if (LongFormAudioSource::prepareMono16k(path, cacheRoot, workingPath,
                                             result.durationMs, &result.error)) {
        result.normalizedPath = workingPath;
    } else if (!LongFormAudioSource::probeDurationMs(path, result.durationMs, &result.error)) {
        return result;
    } else {
        workingPath = path;
    }
    const qint64 chunkMs = qBound<qint64>(qint64(10000),
        settings.value(QStringLiteral("longFormChunkMs"), kDefaultChunkMs).toLongLong(), qint64(30000));
    const qint64 overlapMs = qBound<qint64>(qint64(0),
        settings.value(QStringLiteral("longFormOverlapMs"), kDefaultOverlapMs).toLongLong(), qint64(3000));

    QString vadModel = settings.value(QStringLiteral("vadModelPath")).toString();
    if (vadModel.isEmpty()) vadModel = qEnvironmentVariable("LASTUDIO_VAD_MODEL");
    QString vadRuntimePath = settings.value(QStringLiteral("vadRuntimePath")).toString();
    if (vadRuntimePath.isEmpty()) vadRuntimePath = qEnvironmentVariable("LASTUDIO_CRISPASR_RUNTIME");
    const QString vadRuntime = findRuntimeLibrary(vadRuntimePath);
    const bool useVad = settings.value(QStringLiteral("longFormVadEnabled"), true).toBool()
        && QFileInfo(vadModel).isFile() && QFileInfo(vadRuntime).isFile();
    QVector<SttLongFormChunk> speechChunks;
    if (useVad) {
        CrispAlignmentInterface crisp;
        if (crisp.load(vadRuntime)) {
            const float threshold = float(settings.value(QStringLiteral("vadThreshold"), 0.5).toDouble());
            const int minSpeech = settings.value(QStringLiteral("vadMinSpeechMs"), 250).toInt();
            const int minSilence = settings.value(QStringLiteral("vadMinSilenceMs"), 300).toInt();
            const int pad = settings.value(QStringLiteral("vadSpeechPadMs"), 250).toInt();
            QVector<SttLongFormChunk> spans;
            for (qint64 windowStart = 0; windowStart < result.durationMs; windowStart += kVadWindowMs - 2000) {
                const qint64 windowDuration = qMin(kVadWindowMs, result.durationMs - windowStart);
                QString error;
                const QVector<float> samples = LongFormAudioSource::readMono16kRange(workingPath, windowStart, windowDuration, &error);
                if (samples.isEmpty()) continue;
                const auto local = crisp.vadSlices(vadModel, samples, threshold, minSpeech, minSilence,
                                                    pad, float(chunkMs) / 1000.0f, settings.value(QStringLiteral("threads"), 0).toInt());
                for (const auto &span : local) {
                    const qint64 begin = qMax<qint64>(0, windowStart + qRound64(span.start * 1000.0f) - overlapMs);
                    const qint64 end = qMin(result.durationMs, windowStart + qRound64(span.end * 1000.0f) + overlapMs);
                    if (end > begin) spans.append({begin, end});
                }
            }
            std::sort(spans.begin(), spans.end(), [](const auto &a, const auto &b) { return a.startMs < b.startMs; });
            for (const auto &span : spans) {
                if (!speechChunks.isEmpty() && span.startMs <= speechChunks.back().endMs + 250) {
                    speechChunks.back().endMs = qMax(speechChunks.back().endMs, span.endMs);
                } else {
                    speechChunks.append(span);
                }
            }
        }
    }

    if (speechChunks.isEmpty()) {
        for (qint64 start = 0; start < result.durationMs; start += chunkMs - overlapMs) {
            const qint64 end = qMin(result.durationMs, start + chunkMs);
            result.chunks.append({start, end});
            if (end >= result.durationMs) break;
        }
    } else {
        for (const auto &span : speechChunks) {
            for (qint64 start = span.startMs; start < span.endMs; start += chunkMs - overlapMs) {
                const qint64 end = qMin(span.endMs, start + chunkMs);
                result.chunks.append({start, end});
                if (end >= span.endMs) break;
            }
        }
    }
    if (result.chunks.isEmpty()) result.error = QStringLiteral("No audio chunks were generated.");
    return result;
}

} // namespace

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
        connect(m_engine, &SttEngine::errorOccurred, this, [this](const QString &message) {
            if (m_longFormActive) {
                failLongForm(message);
                return;
            }
            if (!m_activeJob.isValid) return;
            m_activeJob.isValid = false;
            emit transcriptionFailed(message);
        });
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

SttSessionController::~SttSessionController()
{
    m_longFormCancel = true;
    if (m_probeWatcher) m_probeWatcher->cancel();
    if (m_planWatcher) m_planWatcher->cancel();
    if (m_rangeWatcher) m_rangeWatcher->cancel();
}

QString SttSessionController::transcript() const
{
    return m_transcript;
}

bool SttSessionController::processing() const
{
    return m_longFormActive || (m_engine && m_engine->isProcessing());
}

int SttSessionController::progress() const
{
    if (m_longFormActive) return m_totalChunks > 0
        ? qBound(0, int((100LL * m_completedChunks) / m_totalChunks), 99) : 0;
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
    Logger::info(QStringLiteral("SttSession"),
                 QStringLiteral("Audio decode requested path=%1").arg(m_inputPath));
    m_inputUrl = QUrl::fromLocalFile(m_inputPath);
    emit inputPathChanged();
    emit inputUrlChanged();

    m_inputLoading = true;
    emit inputLoadingChanged();

    auto *watcher = new QFutureWatcher<SttProbeResult>(this);
    m_probeWatcher = watcher;
    connect(watcher, &QFutureWatcher<SttProbeResult>::finished,
            this, &SttSessionController::onProbeFinished);
    watcher->setFuture(QtConcurrent::run([path = m_inputPath]() {
        SttProbeResult result;
        LongFormAudioSource::probeDurationMs(path, result.durationMs, &result.error);
        return result;
    }));
}

void SttSessionController::clearInput()
{
    if (!m_inputPath.isEmpty()) QFile::remove(checkpointPath());
    if (m_activeDecoder) {
        m_activeDecoder->disconnect(this);
        m_activeDecoder->deleteLater();
        m_activeDecoder = nullptr;
    }

    if (m_probeWatcher) {
        m_probeWatcher->cancel();
        m_probeWatcher->deleteLater();
        m_probeWatcher = nullptr;
    }
    if (m_planWatcher) {
        m_planWatcher->cancel();
        m_planWatcher->deleteLater();
        m_planWatcher = nullptr;
    }
    if (m_rangeWatcher) {
        m_rangeWatcher->cancel();
        m_rangeWatcher->deleteLater();
        m_rangeWatcher = nullptr;
    }
    m_longFormCancel = true;
    m_longFormActive = false;
    m_longFormMode = false;
    m_longFormAudioPath.clear();
    m_longFormChunks.clear();
    m_longFormSegments.clear();
    m_longFormChunkSamples.clear();
    m_completedChunks = 0;
    m_totalChunks = 0;
    m_processingStage.clear();
    m_resumable = false;
    m_inputDurationMs = 0;

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
    emit inputDurationChanged();
    emit segmentsChanged();
    emit transcriptChanged();
    emit processingStageChanged();
    emit chunkProgressChanged();
    emit resumableChanged();
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
    if (m_inputLoading) {
        Logger::debug(QStringLiteral("SttSession"), QStringLiteral("Transcription deferred while audio decode is still running"));
        return;
    }
    if (!m_engine || (!m_longFormMode && m_decodedSamples.isEmpty())) {
        Logger::warning(QStringLiteral("SttSession"),
                        QStringLiteral("Transcription skipped: engine=%1 samples=%2 inputError=%3")
                            .arg(m_engine ? QStringLiteral("available") : QStringLiteral("missing"))
                            .arg(m_longFormMode ? QStringLiteral("range-backed") : QString::number(m_decodedSamples.size()))
                            .arg(m_inputError));
        return;
    }
    if (!canTranscribe()) {
        const QString message = QStringLiteral("The STT model is not ready. Wait for model loading to finish and try again.");
        Logger::warning(QStringLiteral("SttSession"), message);
        emit transcriptionFailed(message);
        return;
    }

    if (m_longFormMode) {
        if (!m_dynamicSettings.contains(QStringLiteral("alignmentModelPath"))) {
            if (AppController *app = AppController::instance()) {
                const auto configuration = AlignmentRefinementService::resolveConfiguration(app->models(), app->runtimes());
                if (configuration.isValid() && configuration.runtimeKind != QStringLiteral("process")) {
                    QVariantMap settings = m_dynamicSettings;
                    settings.insert(QStringLiteral("alignmentModelPath"), configuration.modelPath);
                    settings.insert(QStringLiteral("alignmentRuntimePath"), configuration.runtimePath);
                    m_dynamicSettings = settings;
                }
            }
        }
        startLongFormPlanning(false);
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

bool SttSessionController::canTranscribe() const
{
    return m_engine && m_engine->state() == SttEngine::Ready;
}

void SttSessionController::cancelProcessing()
{
    if (m_longFormActive) {
        m_longFormCancel = true;
        if (m_engine && m_engine->isProcessing()) m_engine->cancelProcessing();
        m_processingStage = QStringLiteral("cancelled");
        emit processingStageChanged();
        emit processingChanged();
        return;
    }
    if (m_engine) {
        m_engine->cancelProcessing();
    }
}

void SttSessionController::clearTranscript()
{
    m_transcript.clear();
    m_segments.clear();
    emit transcriptChanged();
    emit segmentsChanged();
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

void SttSessionController::onProbeFinished()
{
    if (!m_probeWatcher) return;
    const SttProbeResult result = m_probeWatcher->result();
    m_probeWatcher->deleteLater();
    m_probeWatcher = nullptr;
    if (result.durationMs > 0) {
        m_inputDurationMs = result.durationMs;
        emit inputDurationChanged();
    }
    if (!result.error.isEmpty() || result.durationMs <= 0) {
        // Keep the legacy decoder as a compatibility fallback for containers
        // that ffprobe cannot inspect.
        m_activeDecoder = new SttAudioDecoder(this);
        connect(m_activeDecoder, &SttAudioDecoder::finished, this, &SttSessionController::onDecoderFinished);
        connect(m_activeDecoder, &SttAudioDecoder::errorOccurred, this, &SttSessionController::onDecoderError);
        m_activeDecoder->startDecode(m_inputPath);
        return;
    }
    if (result.durationMs >= kLongFormThresholdMs) {
        m_longFormMode = true;
        m_inputLoading = false;
        m_inputError.clear();
        m_waveformSamples.clear();
        m_processingStage = QStringLiteral("ready");
        emit inputLoadingChanged();
        emit inputErrorChanged();
        emit waveformSamplesChanged();
        emit processingStageChanged();
        return;
    }
    m_activeDecoder = new SttAudioDecoder(this);
    connect(m_activeDecoder, &SttAudioDecoder::finished, this, &SttSessionController::onDecoderFinished);
    connect(m_activeDecoder, &SttAudioDecoder::errorOccurred, this, &SttSessionController::onDecoderError);
    m_activeDecoder->startDecode(m_inputPath);
}

void SttSessionController::onDecoderFinished(const QVector<float> &samples)
{
    Logger::info(QStringLiteral("SttSession"),
                 QStringLiteral("Audio decode finished samples=%1 path=%2").arg(samples.size()).arg(m_inputPath));
    m_decodedSamples = samples;
    if (m_inputDurationMs <= 0) {
        m_inputDurationMs = qRound64(double(samples.size()) / 16000.0 * 1000.0);
        emit inputDurationChanged();
    }
    m_longFormMode = false;
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
    Logger::error(QStringLiteral("SttSession"),
                  QStringLiteral("Audio decode failed path=%1 error=%2").arg(m_inputPath, error));
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
    if (numSamples <= 0) {
        emit transcriptionFailed(QStringLiteral("The recording contains no audio samples."));
        return;
    }
    const int sourceRate = m_recorder ? qMax(1, m_recorder->lastRecordingSampleRate()) : 16000;
    const qint64 recordingDurationMs = qRound64(double(numSamples) / sourceRate * 1000.0);
    const int outputSamples = qMax(1, int(std::llround(double(numSamples) * 16000.0 / sourceRate)));
    m_decodedSamples.resize(outputSamples);
    for (int i = 0; i < outputSamples; ++i) {
        const double sourcePosition = double(i) * sourceRate / 16000.0;
        const int first = qBound(0, int(sourcePosition), numSamples - 1);
        const int second = qMin(first + 1, numSamples - 1);
        const double fraction = sourcePosition - first;
        const float left = static_cast<float>(raw[first]) / 32768.0f;
        const float right = static_cast<float>(raw[second]) / 32768.0f;
        m_decodedSamples[i] = float(left * (1.0 - fraction) + right * fraction);
    }

    updateWaveform(m_decodedSamples);
    m_inputDurationMs = recordingDurationMs;
    m_longFormMode = false;
    emit inputDurationChanged();
    emit waveformSamplesChanged();

    transcribeInput();
}


void SttSessionController::onEngineTranscriptionFinished(const QString &text, const QVariantList &segments)
{
    if (m_longFormActive) {
        const SttLongFormChunk chunk = m_longFormChunks.value(m_nextChunkIndex);
        QVariantList relative = segments;
        CrispAlignmentInterface aligner;
        const bool alignmentRequested = !translate()
            && m_dynamicSettings.value(QStringLiteral("longFormAlignmentEnabled"), true).toBool();
        QString alignmentModel = m_dynamicSettings.value(QStringLiteral("alignmentModelPath")).toString();
        if (alignmentModel.isEmpty()) alignmentModel = qEnvironmentVariable("LASTUDIO_ALIGNMENT_MODEL");
        QString alignmentRuntimePath = m_dynamicSettings.value(QStringLiteral("alignmentRuntimePath")).toString();
        if (alignmentRuntimePath.isEmpty()) alignmentRuntimePath = qEnvironmentVariable("LASTUDIO_CRISPASR_RUNTIME");
        const QString alignmentRuntime = findRuntimeLibrary(alignmentRuntimePath);
        const bool alignmentReady = alignmentRequested && QFileInfo(alignmentModel).isFile()
            && QFileInfo(alignmentRuntime).isFile() && aligner.load(alignmentRuntime);
        if (relative.isEmpty() && !text.trimmed().isEmpty()) {
            relative.append(QVariantMap{{QStringLiteral("text"), text},
                                        {QStringLiteral("start"), 0.0},
                                        {QStringLiteral("end"), double(chunk.endMs - chunk.startMs) / 1000.0}});
        }
        for (const QVariant &entry : relative) {
            QVariantMap segment = entry.toMap();
            const qint64 relativeStart = segment.contains(QStringLiteral("startMs"))
                ? segment.value(QStringLiteral("startMs")).toLongLong()
                : qRound64(segment.value(QStringLiteral("start")).toDouble() * 1000.0);
            const qint64 relativeEnd = segment.contains(QStringLiteral("endMs"))
                ? segment.value(QStringLiteral("endMs")).toLongLong()
                : qRound64(segment.value(QStringLiteral("end")).toDouble() * 1000.0);
            const qint64 start = qBound(chunk.startMs, chunk.startMs + relativeStart, chunk.endMs - 1);
            const qint64 end = qBound(start + 1, chunk.startMs + qMax(relativeStart + 1, relativeEnd), chunk.endMs);
            const QString segmentText = segment.value(QStringLiteral("text"), segment.value(QStringLiteral("sourceText"))).toString().simplified();
            if (segmentText.isEmpty()) continue;
            QVariantMap output{{QStringLiteral("id"), QStringLiteral("asr-%1-%2").arg(m_nextChunkIndex).arg(m_longFormSegments.size())},
                               {QStringLiteral("startMs"), start},
                               {QStringLiteral("endMs"), end},
                               {QStringLiteral("text"), segmentText},
                               {QStringLiteral("timingSource"), QStringLiteral("asr")},
                               {QStringLiteral("alignmentStatus"), QStringLiteral("unrefined")}};
            QVariantList words;
            for (const QVariant &wordEntry : segment.value(QStringLiteral("words")).toList()) {
                QVariantMap word = wordEntry.toMap();
                const qint64 ws = word.contains(QStringLiteral("startMs")) ? word.value(QStringLiteral("startMs")).toLongLong()
                    : qRound64(word.value(QStringLiteral("start")).toDouble() * 1000.0);
                const qint64 we = word.contains(QStringLiteral("endMs")) ? word.value(QStringLiteral("endMs")).toLongLong()
                    : qRound64(word.value(QStringLiteral("end")).toDouble() * 1000.0);
                word.insert(QStringLiteral("startMs"), qBound(start, chunk.startMs + ws, end - 1));
                word.insert(QStringLiteral("endMs"), qBound(start + 1, chunk.startMs + qMax(ws + 1, we), end));
                words.append(word);
            }
            if (alignmentReady && words.isEmpty() && !segmentText.isEmpty()) {
                const auto alignedWords = aligner.align(alignmentModel, segmentText,
                                                        m_longFormChunkSamples,
                                                        chunk.startMs * 10,
                                                        qMax(1, threads()));
                for (const auto &aligned : alignedWords) {
                    words.append(QVariantMap{{QStringLiteral("text"), aligned.text},
                                             {QStringLiteral("startMs"), qRound64(aligned.start * 1000.0)},
                                             {QStringLiteral("endMs"), qRound64(aligned.end * 1000.0)}});
                }
                if (!words.isEmpty()) {
                    const QVariantMap first = words.first().toMap();
                    const QVariantMap last = words.last().toMap();
                    output.insert(QStringLiteral("startMs"), first.value(QStringLiteral("startMs")));
                    output.insert(QStringLiteral("endMs"), last.value(QStringLiteral("endMs")));
                }
            }
            if (!words.isEmpty() && alignmentReady) {
                output.insert(QStringLiteral("timingSource"), QStringLiteral("forced-alignment"));
                output.insert(QStringLiteral("alignmentStatus"), QStringLiteral("aligned"));
            }
            if (!words.isEmpty()) output.insert(QStringLiteral("words"), words);
            const QString normalized = normalizeForCompare(segmentText);
            bool duplicate = false;
            if (!m_longFormSegments.isEmpty()) {
                const QVariantMap previous = m_longFormSegments.last().toMap();
                const qint64 previousEnd = previous.value(QStringLiteral("endMs")).toLongLong();
                duplicate = previousEnd > start && normalizeForCompare(previous.value(QStringLiteral("text")).toString()) == normalized;
                if (duplicate && end > previousEnd) {
                    QVariantMap merged = previous;
                    merged.insert(QStringLiteral("endMs"), end);
                    m_longFormSegments.last() = merged;
                }
            }
            if (!duplicate) m_longFormSegments.append(output);
        }
        ++m_nextChunkIndex;
        m_completedChunks = m_nextChunkIndex;
        m_longFormChunkSamples.clear();
        writeCheckpoint();
        updateLongFormProgress();
        QMetaObject::invokeMethod(this, &SttSessionController::startNextLongFormChunk, Qt::QueuedConnection);
        return;
    }
    if (m_activeJob.isValid && !text.trimmed().isEmpty() && m_historyService) {
        m_historyService->addSttHistoryItem(text, m_activeJob.modelName, m_activeJob.samples);
    }
    m_activeJob.isValid = false;
    m_transcript = text;
    m_segments = SttSubtitleService::cuesFromSegments(segments, m_inputDurationMs);
    emit transcriptChanged();
    emit segmentsChanged();
    emit transcriptionFinished(text, segments);
}

QString SttSessionController::checkpointPath() const
{
    const QString root = PathUtils::cacheDir() + QStringLiteral("/stt-long-form");
    QDir().mkpath(root);
    const QByteArray digest = QCryptographicHash::hash(m_inputPath.toUtf8(), QCryptographicHash::Sha256).toHex();
    return root + QLatin1Char('/') + QString::fromLatin1(digest) + QStringLiteral(".json");
}

void SttSessionController::writeCheckpoint()
{
    if (m_inputPath.isEmpty() || m_longFormChunks.isEmpty()) return;
    QJsonObject object;
    object.insert(QStringLiteral("schemaVersion"), 1);
    object.insert(QStringLiteral("inputPath"), QDir::toNativeSeparators(m_inputPath));
    const QFileInfo sourceInfo(m_inputPath);
    object.insert(QStringLiteral("sourceSize"), double(sourceInfo.size()));
    object.insert(QStringLiteral("sourceMtime"), double(sourceInfo.lastModified().toMSecsSinceEpoch()));
    object.insert(QStringLiteral("normalizedPath"), QDir::toNativeSeparators(m_longFormAudioPath));
    object.insert(QStringLiteral("modelSignature"), m_engine ? m_engine->activeSignature() : QString());
    object.insert(QStringLiteral("language"), language());
    object.insert(QStringLiteral("translate"), translate());
    object.insert(QStringLiteral("durationMs"), double(m_inputDurationMs));
    object.insert(QStringLiteral("nextChunk"), m_nextChunkIndex);
    QJsonArray chunks;
    for (const auto &chunk : m_longFormChunks)
        chunks.append(QJsonObject{{QStringLiteral("startMs"), double(chunk.startMs)},
                                  {QStringLiteral("endMs"), double(chunk.endMs)}});
    object.insert(QStringLiteral("chunks"), chunks);
    object.insert(QStringLiteral("segments"), QJsonArray::fromVariantList(m_longFormSegments));
    QSaveFile file(checkpointPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

bool SttSessionController::loadCheckpoint()
{
    QFile file(checkpointPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("inputPath")).toString() != QDir::toNativeSeparators(m_inputPath)) return false;
    const QFileInfo sourceInfo(m_inputPath);
    if (object.contains(QStringLiteral("sourceSize"))
        && object.value(QStringLiteral("sourceSize")).toDouble() != double(sourceInfo.size())) return false;
    if (object.contains(QStringLiteral("sourceMtime"))
        && object.value(QStringLiteral("sourceMtime")).toDouble() != double(sourceInfo.lastModified().toMSecsSinceEpoch())) return false;
    if (object.contains(QStringLiteral("modelSignature")) && m_engine
        && object.value(QStringLiteral("modelSignature")).toString() != m_engine->activeSignature()) return false;
    if (object.contains(QStringLiteral("language"))
        && object.value(QStringLiteral("language")).toString() != language()) return false;
    if (object.contains(QStringLiteral("translate"))
        && object.value(QStringLiteral("translate")).toBool() != translate()) return false;
    m_inputDurationMs = qRound64(object.value(QStringLiteral("durationMs")).toDouble());
    m_longFormAudioPath = object.value(QStringLiteral("normalizedPath")).toString();
    if (!m_longFormAudioPath.isEmpty() && !QFileInfo(m_longFormAudioPath).isFile())
        m_longFormAudioPath.clear();
    m_longFormChunks.clear();
    for (const auto &value : object.value(QStringLiteral("chunks")).toArray()) {
        const QJsonObject chunk = value.toObject();
        m_longFormChunks.append({qRound64(chunk.value(QStringLiteral("startMs")).toDouble()),
                                 qRound64(chunk.value(QStringLiteral("endMs")).toDouble())});
    }
    m_nextChunkIndex = qBound(0, object.value(QStringLiteral("nextChunk")).toInt(), m_longFormChunks.size());
    m_completedChunks = m_nextChunkIndex;
    m_totalChunks = m_longFormChunks.size();
    m_longFormSegments = object.value(QStringLiteral("segments")).toArray().toVariantList();
    m_resumable = m_nextChunkIndex < m_totalChunks;
    emit inputDurationChanged();
    emit chunkProgressChanged();
    emit resumableChanged();
    return !m_longFormChunks.isEmpty();
}

void SttSessionController::startLongFormPlanning(bool resume)
{
    if (m_longFormActive) return;
    if (resume && loadCheckpoint()) {
        m_longFormCancel = false;
        m_longFormActive = true;
        m_longFormMode = true;
        m_processingStage = QStringLiteral("transcribing");
        emit processingChanged();
        emit processingStageChanged();
        startNextLongFormChunk();
        return;
    }
    m_longFormCancel = false;
    m_longFormActive = true;
    m_processingStage = QStringLiteral("planning");
    m_completedChunks = 0;
    m_totalChunks = 0;
    m_longFormSegments.clear();
    emit processingChanged();
    emit processingStageChanged();
    emit chunkProgressChanged();
    auto *watcher = new QFutureWatcher<SttChunkPlanResult>(this);
    m_planWatcher = watcher;
    connect(watcher, &QFutureWatcher<SttChunkPlanResult>::finished,
            this, &SttSessionController::onPlanFinished);
    const QString path = m_inputPath;
    const QVariantMap settings = m_dynamicSettings;
    watcher->setFuture(QtConcurrent::run([path, settings]() { return buildChunkPlan(path, settings); }));
}

void SttSessionController::onPlanFinished()
{
    if (!m_planWatcher) return;
    const SttChunkPlanResult result = m_planWatcher->result();
    m_planWatcher->deleteLater();
    m_planWatcher = nullptr;
    if (m_longFormCancel) { m_longFormActive = false; emit processingChanged(); return; }
    if (!result.error.isEmpty() || result.chunks.isEmpty()) {
        failLongForm(result.error.isEmpty() ? QStringLiteral("Could not create audio chunks.") : result.error);
        return;
    }
    m_inputDurationMs = result.durationMs;
    m_longFormAudioPath = result.normalizedPath;
    m_longFormChunks = result.chunks;
    m_nextChunkIndex = 0;
    m_completedChunks = 0;
    m_totalChunks = m_longFormChunks.size();
    m_resumable = true;
    m_processingStage = QStringLiteral("transcribing");
    writeCheckpoint();
    emit inputDurationChanged();
    emit processingStageChanged();
    emit chunkProgressChanged();
    emit resumableChanged();
    startNextLongFormChunk();
}

void SttSessionController::startNextLongFormChunk()
{
    if (!m_longFormActive) return;
    if (m_longFormCancel) {
        writeCheckpoint();
        m_longFormActive = false;
        m_resumable = m_nextChunkIndex < m_totalChunks;
        emit processingChanged();
        emit resumableChanged();
        return;
    }
    if (m_nextChunkIndex >= m_longFormChunks.size()) {
        finishLongForm();
        return;
    }
    const SttLongFormChunk chunk = m_longFormChunks.at(m_nextChunkIndex);
    auto *watcher = new QFutureWatcher<SttRangeResult>(this);
    m_rangeWatcher = watcher;
    connect(watcher, &QFutureWatcher<SttRangeResult>::finished,
            this, &SttSessionController::onRangeFinished);
    const QString path = m_longFormAudioPath.isEmpty() ? m_inputPath : m_longFormAudioPath;
    watcher->setFuture(QtConcurrent::run([path, chunk]() {
        SttRangeResult result;
        result.samples = LongFormAudioSource::readMono16kRange(path, chunk.startMs,
                                                               chunk.endMs - chunk.startMs,
                                                               &result.error);
        return result;
    }));
}

void SttSessionController::onRangeFinished()
{
    if (!m_rangeWatcher) return;
    const SttRangeResult result = m_rangeWatcher->result();
    m_rangeWatcher->deleteLater();
    m_rangeWatcher = nullptr;
    if (m_longFormCancel) { startNextLongFormChunk(); return; }
    if (result.samples.isEmpty()) {
        failLongForm(result.error.isEmpty() ? QStringLiteral("Could not read the next audio chunk.") : result.error);
        return;
    }
    m_longFormChunkSamples = result.samples;
    m_engine->transcribeSamples(m_longFormChunkSamples, language(), threads(), translate(), m_dynamicSettings);
}

void SttSessionController::updateLongFormProgress()
{
    emit progressChanged();
    emit chunkProgressChanged();
}

void SttSessionController::finishLongForm()
{
    m_segments = SttSubtitleService::cuesFromSegments(m_longFormSegments, m_inputDurationMs);
    QStringList texts;
    for (const auto &entry : m_segments) texts.append(entry.toMap().value(QStringLiteral("text")).toString());
    m_transcript = texts.join(QStringLiteral(" ")).simplified();
    m_longFormChunkSamples.clear();
    m_longFormActive = false;
    m_resumable = false;
    m_processingStage = QStringLiteral("completed");
    QFile::remove(checkpointPath());
    emit transcriptChanged();
    emit segmentsChanged();
    emit processingStageChanged();
    emit resumableChanged();
    emit progressChanged();
    emit processingChanged();
    emit transcriptionFinished(m_transcript, m_segments);
}

void SttSessionController::failLongForm(const QString &message)
{
    writeCheckpoint();
    m_longFormActive = false;
    m_resumable = m_nextChunkIndex < m_totalChunks;
    m_processingStage = QStringLiteral("error");
    emit processingStageChanged();
    emit processingChanged();
    emit resumableChanged();
    emit transcriptionFailed(message);
}

bool SttSessionController::updateSegment(const QString &id, qint64 startMs, qint64 endMs,
                                          const QString &text)
{
    if (id.isEmpty() || endMs <= startMs) return false;
    for (int i = 0; i < m_segments.size(); ++i) {
        QVariantMap cue = m_segments.at(i).toMap();
        if (cue.value(QStringLiteral("id")).toString() != id) continue;
        cue.insert(QStringLiteral("startMs"), qMax<qint64>(0, startMs));
        cue.insert(QStringLiteral("endMs"), endMs);
        cue.insert(QStringLiteral("text"), text.trimmed());
        cue.insert(QStringLiteral("timingSource"), QStringLiteral("manual"));
        cue.insert(QStringLiteral("alignmentStatus"), QStringLiteral("manual"));
        cue.remove(QStringLiteral("words"));
        m_segments[i] = cue;
        QStringList texts;
        for (const auto &entry : m_segments) texts.append(entry.toMap().value(QStringLiteral("text")).toString());
        m_transcript = texts.join(QStringLiteral(" ")).simplified();
        emit segmentsChanged();
        emit transcriptChanged();
        return true;
    }
    return false;
}

bool SttSessionController::exportSrt(const QString &filePath)
{
    QString error;
    const bool ok = SttSubtitleService::writeSrt(PathUtils::urlToLocalPath(filePath), m_segments, &error);
    if (!ok) emit transcriptionFailed(error);
    return ok;
}

bool SttSessionController::exportVtt(const QString &filePath)
{
    QString error;
    const bool ok = SttSubtitleService::writeVtt(PathUtils::urlToLocalPath(filePath), m_segments, &error);
    if (!ok) emit transcriptionFailed(error);
    return ok;
}

void SttSessionController::resumeProcessing()
{
    if (m_resumable) startLongFormPlanning(true);
}

void SttSessionController::discardCheckpoint()
{
    QFile::remove(checkpointPath());
    m_resumable = false;
    emit resumableChanged();
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
