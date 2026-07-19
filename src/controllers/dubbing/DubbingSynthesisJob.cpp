#include "controllers/dubbing/DubbingSynthesisJob.h"

#include "audio/WavIO.h"
#include "core/Logger.h"
#include "dubbing/DubbingTimingService.h"
#include "tts/TtsEngine.h"
#include "workflows/WorkflowArtifact.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QtMath>

namespace LAStudio {
namespace {

QVariantList waveformPreview(const QVector<float> &samples, int maximumPoints = 360)
{
    QVariantList preview;
    if (samples.isEmpty() || maximumPoints <= 0) return preview;
    const int pointCount = qMin(samples.size(), maximumPoints);
    preview.reserve(pointCount);
    for (int point = 0; point < pointCount; ++point) {
        const int begin = point * samples.size() / pointCount;
        const int end = qMax(begin + 1, (point + 1) * samples.size() / pointCount);
        float peak = 0.0f;
        for (int sample = begin; sample < end; ++sample) peak = qMax(peak, qAbs(samples.at(sample)));
        preview.append(peak);
    }
    return preview;
}

QString fingerprint(const QVariantMap &segment, const QString &ttsSignature,
                    const QVariantMap &settings)
{
    const QVariantMap value{{QStringLiteral("targetText"), segment.value(QStringLiteral("targetText"))},
                            {QStringLiteral("startMs"), segment.value(QStringLiteral("startMs"))},
                            {QStringLiteral("endMs"), segment.value(QStringLiteral("endMs"))},
                            {QStringLiteral("speakerId"), segment.value(QStringLiteral("speakerId"))},
                            {QStringLiteral("ttsSignature"), ttsSignature},
                            {QStringLiteral("synthesisSettings"), settings}};
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(QJsonObject::fromVariantMap(value)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}

bool needsSynthesis(const QVariantMap &segment, const QString &signature,
                    const QVariantMap &settings)
{
    if (segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty()) return false;
    return segment.value(QStringLiteral("state")).toString() != QStringLiteral("ready")
        || !QFileInfo::exists(segment.value(QStringLiteral("clipPath")).toString())
        || segment.value(QStringLiteral("cacheFingerprint")).toString() != fingerprint(segment, signature, settings);
}
}

DubbingSynthesisJob::DubbingSynthesisJob(TtsEngine *tts, QObject *parent)
    : QObject(parent), m_tts(tts)
{
    if (!m_tts) return;
    connect(m_tts, &TtsEngine::synthesisFinished,
            this, &DubbingSynthesisJob::onSynthesisFinished);
    connect(m_tts, &TtsEngine::errorOccurred,
            this, &DubbingSynthesisJob::onTtsError);
}

bool DubbingSynthesisJob::start(const QVariantList &segments, const QString &projectPath,
                                const QVariantMap &settings, const QString &runId)
{
    if (m_running || (m_tts && m_tts->isProcessing())) {
        fail(QStringLiteral("Speech synthesis is already running."));
        return false;
    }
    if (!m_tts || !m_tts->isModelLoaded()) {
        fail(QStringLiteral("Load a TTS model before generating dubbing audio."));
        return false;
    }
    m_segments = segments;
    m_projectPath = projectPath;
    m_settings = settings;
    m_runId = runId;
    m_generationIndex = -1;
    bool hasTargetText = false;
    for (int i = 0; i < m_segments.size(); ++i) {
        const QVariantMap segment = m_segments.at(i).toMap();
        hasTargetText = hasTargetText || !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
        if (needsSynthesis(segment, m_tts->activeSignature(), m_settings)) {
            m_generationIndex = i;
            break;
        }
    }
    if (m_generationIndex < 0) {
        if (!hasTargetText) { fail(QStringLiteral("Add target text to at least one segment before generating.")); return false; }
        m_running = true;
        emit progressChanged(100);
        fitGeneratedSegments();
        return true;
    }
    m_running = true;
    m_nodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_chunkIndex = -1;
    emit progressChanged(0);
    startCurrentChunk();
    return true;
}

void DubbingSynthesisJob::cancel()
{
    if (!m_running) return;
    if (m_tts && m_tts->isProcessing()) m_tts->cancelProcessing();
    m_running = false;
    m_generationIndex = -1;
}

void DubbingSynthesisJob::startCurrentChunk()
{
    if (!m_running || !m_tts || m_generationIndex < 0 || m_generationIndex >= m_segments.size()) return;
    if (m_chunkIndex < 0) {
        const QVariantMap segment = m_segments.at(m_generationIndex).toMap();
        m_chunks = segment.value(QStringLiteral("targetChunks")).toList();
        if (m_chunks.isEmpty()) m_chunks.append(QVariantMap{{QStringLiteral("text"), segment.value(QStringLiteral("targetText"))},
                                                             {QStringLiteral("pauseAfterMs"), 0},
                                                             {QStringLiteral("leadingPauseMs"), 0}});
        m_chunkSamples.clear();
        m_chunkSampleRate = 0;
        m_chunkIndex = 0;
    }
    const QString text = m_chunks.at(m_chunkIndex).toMap().value(QStringLiteral("text")).toString().trimmed();
    if (text.isEmpty()) { fail(QStringLiteral("Pause alignment produced an empty TTS chunk.")); return; }
    m_tts->synthesize(text, 0, 1.0f, m_settings);
}

void DubbingSynthesisJob::onSynthesisFinished(const QByteArray &pcm16, int sampleRate)
{
    Q_UNUSED(pcm16);
    if (!m_running || m_generationIndex < 0 || m_generationIndex >= m_segments.size()) return;
    QVector<float> samples = m_tts ? m_tts->lastSamples() : QVector<float>();
    if (samples.isEmpty() || sampleRate <= 0) { fail(QStringLiteral("TTS returned empty audio for segment %1.").arg(m_generationIndex + 1)); return; }
    if (m_chunkIndex >= 0 && m_chunkIndex < m_chunks.size()) {
        const QVariantMap chunk = m_chunks.at(m_chunkIndex).toMap();
        if (m_chunkSampleRate == 0) m_chunkSampleRate = sampleRate;
        if (sampleRate != m_chunkSampleRate) { fail(QStringLiteral("TTS changed sample rate between pause-aligned chunks.")); return; }
        if (m_chunkIndex == 0) m_chunkSamples.append(QVector<float>(qMax<qint64>(0, chunk.value(QStringLiteral("leadingPauseMs")).toLongLong() * sampleRate / 1000), 0.0f));
        m_chunkSamples.append(samples);
        m_chunkSamples.append(QVector<float>(qMax<qint64>(0, chunk.value(QStringLiteral("pauseAfterMs")).toLongLong() * sampleRate / 1000), 0.0f));
        if (m_chunkIndex + 1 < m_chunks.size()) {
            ++m_chunkIndex;
            QMetaObject::invokeMethod(this, [this]() { startCurrentChunk(); }, Qt::QueuedConnection);
            return;
        }
        samples = m_chunkSamples;
        sampleRate = m_chunkSampleRate;
        m_chunkIndex = -1;
        m_chunks.clear();
        m_chunkSamples.clear();
        m_chunkSampleRate = 0;
    }

    const QVariantMap segment = m_segments.at(m_generationIndex).toMap();
    const qint64 slotMs = qMax<qint64>(1, segment.value(QStringLiteral("endMs")).toLongLong() - segment.value(QStringLiteral("startMs")).toLongLong());
    const qint64 sourceDurationMs = qMax<qint64>(1, qRound64(samples.size() * 1000.0 / sampleRate));
    const double fitFactor = static_cast<double>(sourceDurationMs) / slotMs;
    const bool conflict = fitFactor < 0.85 || fitFactor > 1.20;
    WorkflowArtifactStore store(QDir(QFileInfo(m_projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
    QString error;
    const QString staging = store.createStagingDirectory(m_runId, m_nodeRunId, &error);
    if (staging.isEmpty()) { fail(error); return; }
    const QString stagedClip = QDir(staging).filePath(QStringLiteral("clip.wav"));
    if (!WavIO::saveFloat(stagedClip, samples.constData(), samples.size(), sampleRate)) { fail(QStringLiteral("Failed to stage generated clip: %1").arg(stagedClip)); return; }
    WorkflowArtifactReference artifact;
    if (!store.commitFile(stagedClip, QStringLiteral("audio.clip@1"), m_runId, m_nodeRunId, artifact, &error)) { fail(error); return; }
    const QString clipPath = store.resolve(artifact);
    if (clipPath.isEmpty()) { fail(QStringLiteral("Committed clip artifact cannot be resolved.")); return; }

    QVariantMap updated = segment;
    updated.insert(QStringLiteral("clipPath"), clipPath);
    updated.insert(QStringLiteral("clipArtifact"), artifact.toJson().toVariantMap());
    updated.insert(QStringLiteral("cacheFingerprint"), fingerprint(segment, m_tts->activeSignature(), m_settings));
    updated.insert(QStringLiteral("sampleRate"), sampleRate);
    updated.insert(QStringLiteral("sampleCount"), samples.size());
    updated.insert(QStringLiteral("waveformSamples"), waveformPreview(samples));
    updated.insert(QStringLiteral("sourceDurationMs"), sourceDurationMs);
    updated.insert(QStringLiteral("durationMs"), sourceDurationMs);
    updated.insert(QStringLiteral("fitFactor"), fitFactor);
    updated.insert(QStringLiteral("timingConflict"), conflict);
    updated.insert(QStringLiteral("fitMethod"), QStringLiteral("pending"));
    updated.insert(QStringLiteral("state"), conflict ? QStringLiteral("conflict") : QStringLiteral("natural"));
    m_segments[m_generationIndex] = updated;
    emit segmentUpdated(m_generationIndex, updated);

    int next = m_generationIndex + 1;
    while (next < m_segments.size() && !needsSynthesis(m_segments.at(next).toMap(), m_tts->activeSignature(), m_settings)) ++next;
    if (next >= m_segments.size()) { m_generationIndex = -1; fitGeneratedSegments(); return; }
    m_generationIndex = next;
    m_nodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    emit progressChanged(qRound((100.0 * next) / m_segments.size()));
    QMetaObject::invokeMethod(this, [this]() { startCurrentChunk(); }, Qt::QueuedConnection);
}

void DubbingSynthesisJob::fitGeneratedSegments()
{
    QString error;
    const QVariantList fitted = DubbingTimingService::fitSegments(m_segments, nullptr, &error);
    if (!error.isEmpty()) { fail(error); return; }
    m_segments = fitted;
    m_running = false;
    emit progressChanged(100);
    emit completed(m_segments);
}

void DubbingSynthesisJob::onTtsError(const QString &message)
{
    if (m_running && m_generationIndex >= 0) fail(message);
}

void DubbingSynthesisJob::fail(const QString &message)
{
    m_running = false;
    m_generationIndex = -1;
    emit failed(message);
}

} // namespace LAStudio
