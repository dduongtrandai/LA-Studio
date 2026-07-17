#include "TimedSpeechPipeline.h"

#include "SubtitleSmartFitPlanner.h"
#include "audio/AudioTimelineRenderer.h"
#include "audio/WavIO.h"
#include "core/PathUtils.h"
#include "tts/TtsEngine.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtMath>

namespace LAStudio {

namespace {

QVariantList buildWaveformPreview(const QVector<float> &samples, int maximumPoints = 720)
{
    QVariantList preview;
    if (samples.isEmpty() || maximumPoints <= 0) return preview;

    const int pointCount = qMin(samples.size(), maximumPoints);
    preview.reserve(pointCount);
    for (int point = 0; point < pointCount; ++point) {
        const int begin = point * samples.size() / pointCount;
        const int end = qMax(begin + 1, (point + 1) * samples.size() / pointCount);
        float peak = 0.0f;
        for (int sample = begin; sample < end; ++sample)
            peak = qMax(peak, qAbs(samples.at(sample)));
        preview.append(peak);
    }
    return preview;
}

} // namespace

TimedSpeechPipeline::TimedSpeechPipeline(TtsEngine *tts, QObject *parent)
    : QObject(parent), m_tts(tts)
{
    if (m_tts) {
        connect(m_tts, &TtsEngine::synthesisFinished,
                this, &TimedSpeechPipeline::onTtsFinished);
        connect(m_tts, &TtsEngine::errorOccurred,
                this, &TimedSpeechPipeline::onTtsError);
    }
}

void TimedSpeechPipeline::setPhase(const QString &phase)
{
    if (m_phase == phase) return;
    m_phase = phase;
    emit phaseChanged(m_phase);
}

void TimedSpeechPipeline::resetJobDirectory()
{
    m_jobDirectory = std::make_unique<QTemporaryDir>(
        PathUtils::cacheDir() + QStringLiteral("/subtitle-voice-XXXXXX"));
    if (!m_jobDirectory->isValid()) m_jobDirectory.reset();
}

bool TimedSpeechPipeline::start(const QVector<TimedTextCue> &cues, const QVariantMap &settings)
{
    if (m_processing || cues.isEmpty() || !m_tts || !m_tts->isModelLoaded()) return false;
    resetJobDirectory();
    if (!m_jobDirectory) {
        emit errorOccurred(QStringLiteral("Cannot create temporary subtitle voice directory."));
        return false;
    }
    m_cues = cues;
    m_ttsSettings = settings;
    m_ttsSignature = m_tts->activeSignature();
    m_naturalPaths = QVector<QString>(m_cues.size());
    m_naturalDurationsMs = QVector<qint64>(m_cues.size(), 0);
    m_initialFits = SubtitleSmartFitPlanner::plan(m_cues, m_naturalDurationsMs);
    m_sampleRate = 0;
    m_currentCue = -1;
    m_cancelRequested = false;
    m_processing = true;
    setPhase(QStringLiteral("synthesizing"));
    emit progressChanged(0);
    startCue(0);
    return true;
}

void TimedSpeechPipeline::startCue(int index)
{
    if (m_cancelRequested) return;
    if (index >= m_cues.size()) { fitAndAssemble(); return; }
    if (!m_tts->isModelLoaded() || m_tts->activeSignature() != m_ttsSignature) {
        m_processing = false;
        setPhase(QStringLiteral("error"));
        emit errorOccurred(QStringLiteral("The active TTS model changed during generation."));
        return;
    }

    m_currentCue = index;
    if (m_initialFits.at(index).droppedOverlap) {
        updateCue(index, {{QStringLiteral("state"), QStringLiteral("dropped_overlap")},
                          {QStringLiteral("fitStatus"), QStringLiteral("dropped_overlap")} });
        const int next = index + 1;
        m_currentCue = next;
        emit progressChanged(qRound(70.0 * next / qMax(1, m_cues.size())));
        startCue(next);
        return;
    }

    updateCue(index, {{QStringLiteral("state"), QStringLiteral("generating")} });
    emit progressChanged(qRound(70.0 * index / qMax(1, m_cues.size())));
    m_awaitingSynthesis = true;
    m_tts->synthesize(m_cues.at(index).text, 0, 1.0f, m_ttsSettings);
}

void TimedSpeechPipeline::onTtsFinished(const QByteArray &, int sampleRate)
{
    if (!m_processing || m_currentCue < 0 || m_currentCue >= m_cues.size()) return;
    if (m_tts->activeSignature() != m_ttsSignature) {
        m_processing = false;
        setPhase(QStringLiteral("error"));
        emit errorOccurred(QStringLiteral("The active TTS model changed during generation."));
        return;
    }
    const QVector<float> samples = m_tts->lastSamples();
    // TtsEngine emits an empty synthesisFinished signal synchronously when a
    // request starts, clearing the previous output buffer. It is not a failed
    // synthesis result and must not advance the subtitle queue.
    if (m_awaitingSynthesis && samples.isEmpty()) return;
    m_awaitingSynthesis = false;
    const QString path = QDir(m_jobDirectory->path()).filePath(
        QStringLiteral("natural-%1.wav").arg(m_currentCue));
    if (samples.isEmpty() || sampleRate <= 0
        || !WavIO::saveFloat(path, samples.constData(), samples.size(), sampleRate)) {
        updateCue(m_currentCue, {{QStringLiteral("state"), QStringLiteral("failed_silence")},
                                 {QStringLiteral("error"), QStringLiteral("TTS returned empty audio or could not be saved.")} });
        m_naturalDurationsMs[m_currentCue] = 0;
    } else {
        m_sampleRate = sampleRate;
        m_naturalPaths[m_currentCue] = path;
        m_naturalDurationsMs[m_currentCue] = qRound64(samples.size() * 1000.0 / sampleRate);
        updateCue(m_currentCue, {{QStringLiteral("naturalDurationMs"), m_naturalDurationsMs[m_currentCue]},
                                 {QStringLiteral("outputDurationMs"), m_naturalDurationsMs[m_currentCue]},
                                 {QStringLiteral("audioPath"), path},
                                 {QStringLiteral("waveformSamples"), buildWaveformPreview(samples)},
                                 {QStringLiteral("sampleRate"), sampleRate},
                                 {QStringLiteral("state"), QStringLiteral("generated")} });
    }
    const int next = m_currentCue + 1;
    m_currentCue = next;
    // TtsEngineInstance emits synthesisFinished before it transitions from
    // Processing back to Ready. Queue the next request so it is not discarded
    // by the state machine's Processing catch-all handler.
    QMetaObject::invokeMethod(this, [this, next]() { startCue(next); },
                              Qt::QueuedConnection);
}

void TimedSpeechPipeline::onTtsError(const QString &message)
{
    if (!m_processing || m_currentCue < 0 || m_currentCue >= m_cues.size()) return;
    m_awaitingSynthesis = false;
    updateCue(m_currentCue, {{QStringLiteral("state"), QStringLiteral("failed_silence")},
                             {QStringLiteral("error"), message} });
    m_naturalDurationsMs[m_currentCue] = 0;
    const int next = m_currentCue + 1;
    m_currentCue = next;
    QMetaObject::invokeMethod(this, [this, next]() { startCue(next); },
                              Qt::QueuedConnection);
}

void TimedSpeechPipeline::updateCue(int index, const QVariantMap &patch)
{
    if (index >= 0 && index < m_cues.size()) emit cueUpdated(index, patch);
}

void TimedSpeechPipeline::fitAndAssemble()
{
    if (m_cancelRequested) return;
    if (m_sampleRate <= 0) m_sampleRate = m_tts ? m_tts->sampleRate() : 22050;
    if (m_sampleRate <= 0) m_sampleRate = 22050;
    setPhase(QStringLiteral("fitting"));
    const QVector<SubtitleFit> fits = SubtitleSmartFitPlanner::plan(m_cues, m_naturalDurationsMs);
    QVector<QString> fittedPaths(m_cues.size());
    int slowed = 0, stretched = 0, trimmed = 0, failed = 0, dropped = 0;

    for (int i = 0; i < m_cues.size(); ++i) {
        if (m_cancelRequested) return;
        const SubtitleFit &fit = fits.at(i);
        QVariantMap patch{{QStringLiteral("scheduledStartMs"), fit.scheduledStartMs},
                          {QStringLiteral("effectiveEndMs"), fit.effectiveEndMs},
                          {QStringLiteral("slotDurationMs"), fit.slotMs},
                          {QStringLiteral("audioRate"), fit.audioRate},
                          {QStringLiteral("outputDurationMs"), fit.outputMs},
                          {QStringLiteral("overflowMs"), fit.overflowMs},
                          {QStringLiteral("fitStatus"), fit.status}};
        if (fit.droppedOverlap) {
            patch.insert(QStringLiteral("state"), QStringLiteral("dropped_overlap"));
            ++dropped;
        } else if (m_naturalPaths.at(i).isEmpty()) {
            patch.insert(QStringLiteral("state"), QStringLiteral("failed_silence"));
            ++failed;
        } else {
            const QString output = QDir(m_jobDirectory->path()).filePath(
                QStringLiteral("fitted-%1.wav").arg(i));
            AudioRenderResult renderResult;
            QString renderError;
            const bool ok = AudioTimelineRenderer::renderClip(
                m_naturalPaths.at(i), output, m_sampleRate,
                qMax(1, qRound64(fit.outputMs * m_sampleRate / 1000.0)), fit.audioRate,
                &renderResult, &renderError);
            if (!ok) {
                patch.insert(QStringLiteral("state"), QStringLiteral("failed_silence"));
                patch.insert(QStringLiteral("error"), renderError);
                ++failed;
            } else {
                fittedPaths[i] = output;
                patch.insert(QStringLiteral("audioPath"), output);
                patch.insert(QStringLiteral("state"), QStringLiteral("ready"));
                const WavIO::WavData fittedAudio = WavIO::loadAsFloat(output);
                patch.insert(QStringLiteral("waveformSamples"),
                             buildWaveformPreview(fittedAudio.samples));
                patch.insert(QStringLiteral("sampleRate"), fittedAudio.sampleRate);
                if (renderResult.usedFallback)
                    patch.insert(QStringLiteral("warning"), QStringLiteral("FFmpeg atempo unavailable; linear fallback used."));
                if (fit.status == QStringLiteral("audio_slowed")) ++slowed;
                if (fit.status == QStringLiteral("audio_stretched")) ++stretched;
                if (fit.overflowMs > 0) ++trimmed;
            }
        }
        updateCue(i, patch);
        emit progressChanged(70 + qRound(20.0 * (i + 1) / qMax(1, m_cues.size())));
    }

    setPhase(QStringLiteral("assembling"));
    const QString output = QDir(m_jobDirectory->path()).filePath(QStringLiteral("subtitle-voice.wav"));
    QVector<AudioTimelinePlacement> placements;
    placements.reserve(fits.size());
    for (const SubtitleFit &fit : fits)
        placements.append({fit.scheduledStartMs, fit.effectiveEndMs, !fit.droppedOverlap});
    QString assembleError;
    if (!AudioTimelineRenderer::assemble(fittedPaths, placements, output, m_sampleRate, &assembleError)) {
        m_processing = false;
        setPhase(QStringLiteral("error"));
        emit errorOccurred(assembleError.isEmpty() ? QStringLiteral("Could not assemble final WAV.") : assembleError);
        return;
    }

    qint64 durationMs = 0;
    for (const SubtitleFit &fit : fits) durationMs = qMax(durationMs, fit.effectiveEndMs);
    const QVariantMap summary{{QStringLiteral("totalCues"), m_cues.size()},
                              {QStringLiteral("slowedCues"), slowed},
                              {QStringLiteral("stretchedCues"), stretched},
                              {QStringLiteral("trimmedCues"), trimmed},
                              {QStringLiteral("failedCues"), failed},
                              {QStringLiteral("droppedOverlaps"), dropped},
                              {QStringLiteral("durationMs"), durationMs}};
    m_processing = false;
    setPhase(QStringLiteral("ready"));
    emit progressChanged(100);
    emit finished(output, summary);
}

void TimedSpeechPipeline::cancel()
{
    if (!m_processing) return;
    m_cancelRequested = true;
    if (m_tts) m_tts->cancelProcessing();
    m_processing = false;
    setPhase(QStringLiteral("cancelled"));
}

} // namespace LAStudio
