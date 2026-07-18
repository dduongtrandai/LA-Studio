#include "SubtitleVoiceController.h"

#include "audio/AudioPlayer.h"
#include "audio/WavIO.h"
#include "controllers/shared/HistoryService.h"
#include "core/PathUtils.h"
#include "subtitles/SrtTimelineParser.h"
#include "tts/TimedSpeechPipeline.h"
#include "tts/TtsEngine.h"

#include <QFile>
#include <QFileInfo>

namespace LAStudio {

SubtitleVoiceController::SubtitleVoiceController(TtsEngine *tts, AudioPlayer *player,
                                                 HistoryService *history, QObject *parent)
    : QObject(parent), m_tts(tts), m_player(player), m_history(history)
{
    m_pipeline = new TimedSpeechPipeline(m_tts, this);
    connect(m_pipeline, &TimedSpeechPipeline::cueUpdated,
            this, &SubtitleVoiceController::onPipelineCueUpdated);
    connect(m_pipeline, &TimedSpeechPipeline::phaseChanged,
            this, &SubtitleVoiceController::onPipelinePhaseChanged);
    connect(m_pipeline, &TimedSpeechPipeline::progressChanged,
            this, &SubtitleVoiceController::onPipelineProgressChanged);
    connect(m_pipeline, &TimedSpeechPipeline::errorOccurred,
            this, &SubtitleVoiceController::onPipelineError);
    connect(m_pipeline, &TimedSpeechPipeline::finished,
            this, &SubtitleVoiceController::onPipelineFinished);
    if (m_tts) {
        connect(m_tts, &TtsEngine::stateChanged, this, &SubtitleVoiceController::ttsReadyChanged);
        connect(m_tts, &TtsEngine::activeSignatureChanged, this, &SubtitleVoiceController::ttsReadyChanged);
    }
    if (m_player) {
        connect(m_player, &AudioPlayer::playingChanged, this, [this]() {
            if (!m_player->isPlaying() && m_activePlaybackIndex != -1) {
                m_activePlaybackIndex = -1;
                emit activePlaybackIndexChanged();
            }
        });
    }
}

bool SubtitleVoiceController::ttsReady() const
{
    return m_tts && m_tts->isModelLoaded() && !m_tts->activeSignature().isEmpty();
}

void SubtitleVoiceController::setError(const QString &error)
{
    m_error = error;
    emit errorChanged();
}

bool SubtitleVoiceController::importSrt(const QString &path)
{
    if (m_processing) return false;
    const QString localPath = PathUtils::urlToLocalPath(path);
    const SubtitleParseResult parsed = SrtTimelineParser::parseFile(localPath);
    if (!parsed.ok) {
        setError(parsed.error);
        return false;
    }
    stopPlayback();
    m_sourcePath = QFileInfo(localPath).absoluteFilePath();
    m_typedCues = parsed.cues;
    m_cues.clear();
    for (const TimedTextCue &cue : m_typedCues) m_cues.append(cue.toVariantMap());
    m_outputPath.clear();
    m_summary = QVariantMap{{QStringLiteral("totalCues"), m_cues.size()},
                            {QStringLiteral("skippedCues"), parsed.skippedCues}};
    setError(QString());
    m_phase = QStringLiteral("imported");
    emit sourcePathChanged();
    emit cuesChanged();
    emit outputPathChanged();
    emit summaryChanged();
    emit phaseChanged();
    return true;
}

void SubtitleVoiceController::generate(const QVariantMap &ttsSettings)
{
    if (m_processing || m_typedCues.isEmpty()) return;
    if (!ttsReady()) {
        setError(QStringLiteral("No active TTS model. Load a TTS model before generating voice."));
        m_phase = QStringLiteral("error");
        emit phaseChanged();
        return;
    }
    m_outputPath.clear();
    const QVariantMap familyConfig = m_tts->familyConfig();
    m_historyModelName = familyConfig.value(
        QStringLiteral("title"),
        familyConfig.value(QStringLiteral("name"),
                           familyConfig.value(QStringLiteral("id"),
                                              QStringLiteral("TTS")))).toString();
    m_historyVoiceName = ttsSettings.value(QStringLiteral("voice"),
                                           QStringLiteral("Default")).toString();
    setError(QString());
    m_processing = true;
    m_currentCue = -1;
    emit processingChanged();
    emit outputPathChanged();
    if (!m_pipeline->start(m_typedCues, ttsSettings)) {
        m_processing = false;
        setError(QStringLiteral("Could not start timed speech pipeline."));
        emit processingChanged();
    }
}

void SubtitleVoiceController::updateCue(int index, const QVariantMap &patch)
{
    if (index < 0 || index >= m_cues.size()) return;
    QVariantMap cue = m_cues.at(index).toMap();
    for (auto it = patch.cbegin(); it != patch.cend(); ++it) cue.insert(it.key(), it.value());
    m_cues[index] = cue;
    emit cuesChanged();
}

void SubtitleVoiceController::onPipelineCueUpdated(int index, const QVariantMap &patch)
{
    m_currentCue = index;
    updateCue(index, patch);
    emit progressChanged();
}

void SubtitleVoiceController::onPipelinePhaseChanged(const QString &phase)
{
    if (m_phase == phase) return;
    m_phase = phase;
    emit phaseChanged();
}

void SubtitleVoiceController::onPipelineProgressChanged(int progress)
{
    m_progress = progress;
    emit progressChanged();
}

void SubtitleVoiceController::onPipelineError(const QString &message)
{
    setError(message);
    if (m_pipeline->processing()) return;
    m_processing = false;
    emit processingChanged();
}

void SubtitleVoiceController::onPipelineFinished(const QString &outputPath, const QVariantMap &summary)
{
    m_outputPath = outputPath;
    m_summary = summary;
    m_processing = false;
    if (m_history && QFileInfo::exists(m_outputPath)) {
        const WavIO::WavData audio = WavIO::loadAsFloat(m_outputPath);
        const QString historyText = QStringLiteral("%1 · %2 subtitles")
                                        .arg(QFileInfo(m_sourcePath).completeBaseName())
                                        .arg(m_typedCues.size());
        m_history->addTtsHistorySamples(historyText, m_historyModelName,
                                        m_historyVoiceName, audio.samples, audio.sampleRate);
    }
    emit outputPathChanged();
    emit summaryChanged();
    emit processingChanged();
}

void SubtitleVoiceController::cancel()
{
    if (!m_processing) return;
    m_pipeline->cancel();
    m_processing = false;
    emit processingChanged();
}

void SubtitleVoiceController::clear()
{
    cancel();
    stopPlayback();
    m_sourcePath.clear();
    m_typedCues.clear();
    m_cues.clear();
    m_outputPath.clear();
    m_summary.clear();
    m_error.clear();
    m_currentCue = -1;
    m_progress = 0;
    m_phase = QStringLiteral("idle");
    emit sourcePathChanged();
    emit cuesChanged();
    emit outputPathChanged();
    emit summaryChanged();
    emit errorChanged();
    emit progressChanged();
    emit phaseChanged();
}

bool SubtitleVoiceController::saveOutput(const QString &destination)
{
    if (m_outputPath.isEmpty() || !QFileInfo::exists(m_outputPath)) return false;
    const QString path = PathUtils::urlToLocalPath(destination);
    if (path.isEmpty()) return false;
    QFile::remove(path);
    return QFile::copy(m_outputPath, path);
}

void SubtitleVoiceController::playCue(int index)
{
    if (!m_player || index < 0 || index >= m_cues.size()) return;
    const QString path = m_cues.at(index).toMap().value(QStringLiteral("audioPath")).toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) return;
    m_player->playFile(path);
    if (m_player->isPlaying() && m_activePlaybackIndex != index) {
        m_activePlaybackIndex = index;
        emit activePlaybackIndexChanged();
    }
}

void SubtitleVoiceController::playOutput()
{
    if (!m_player || !QFileInfo::exists(m_outputPath)) return;
    m_player->playFile(m_outputPath);
    if (m_player->isPlaying() && m_activePlaybackIndex != -2) {
        m_activePlaybackIndex = -2;
        emit activePlaybackIndexChanged();
    }
}

void SubtitleVoiceController::pausePlayback()
{
    if (m_player && m_activePlaybackIndex != -1) m_player->pause();
}

void SubtitleVoiceController::resumePlayback()
{
    if (m_player && m_activePlaybackIndex != -1) m_player->resume();
}

void SubtitleVoiceController::seekPlayback(qint64 positionMs)
{
    if (m_player && m_activePlaybackIndex != -1) m_player->seek(positionMs);
}

void SubtitleVoiceController::stopPlayback()
{
    if (m_player) m_player->stop();
    if (m_activePlaybackIndex != -1) {
        m_activePlaybackIndex = -1;
        emit activePlaybackIndexChanged();
    }
}

} // namespace LAStudio
