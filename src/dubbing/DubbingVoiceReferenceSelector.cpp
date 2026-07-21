#include "dubbing/DubbingVoiceReferenceSelector.h"

#include "audio/AudioFileDecoder.h"
#include "audio/WavIO.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace LAStudio {
namespace {

constexpr qint64 MinimumReferenceMs = 3000;
constexpr qint64 MaximumReferenceMs = 15000;
constexpr qint64 MaximumJoinGapMs = 700;

struct Candidate
{
    qint64 startMs = 0;
    qint64 endMs = 0;
    QString text;
    QString speakerId;
    double score = 0.0;
};

double clamp01(double value)
{
    return qBound(0.0, value, 1.0);
}

double signalQuality(const QVector<float> &samples, int sampleRate)
{
    if (samples.isEmpty() || sampleRate <= 0) return 0.0;

    double sumSquares = 0.0;
    double sum = 0.0;
    int active = 0;
    int clipped = 0;
    for (float value : samples) {
        const double sample = value;
        const double absolute = qAbs(sample);
        sumSquares += sample * sample;
        sum += sample;
        if (absolute >= 0.02) ++active;
        if (absolute >= 0.985) ++clipped;
    }

    const double rms = std::sqrt(sumSquares / samples.size());
    const double rmsDb = 20.0 * std::log10(qMax(rms, 1.0e-7));
    const double activeRatio = static_cast<double>(active) / samples.size();
    const double clippingRatio = static_cast<double>(clipped) / samples.size();
    const double dcOffset = qAbs(sum / samples.size());

    const int frameSize = qMax(1, sampleRate / 50); // 20 ms
    QVector<double> frameRms;
    frameRms.reserve((samples.size() + frameSize - 1) / frameSize);
    for (int begin = 0; begin < samples.size(); begin += frameSize) {
        const int end = qMin(samples.size(), begin + frameSize);
        double frameSquares = 0.0;
        for (int i = begin; i < end; ++i) frameSquares += samples.at(i) * samples.at(i);
        frameRms.append(std::sqrt(frameSquares / qMax(1, end - begin)));
    }
    std::sort(frameRms.begin(), frameRms.end());
    const int noiseIndex = qBound(0, frameRms.size() / 5, frameRms.size() - 1);
    const double noiseFloor = frameRms.at(noiseIndex);
    const double snrDb = 20.0 * std::log10(qMax(rms, 1.0e-7) / qMax(noiseFloor, 1.0e-6));

    const double loudness = clamp01(1.0 - qAbs(rmsDb + 20.0) / 28.0);
    const double speechDensity = clamp01(activeRatio / 0.55);
    const double noiseRejection = clamp01((snrDb - 4.0) / 24.0);
    const double cleanSignal = clamp01(1.0 - clippingRatio * 80.0 - dcOffset * 5.0);
    return 0.30 * loudness + 0.25 * speechDensity
        + 0.30 * noiseRejection + 0.15 * cleanSignal;
}

QVector<Candidate> buildCandidates(const QVariantList &segments, const QString &speakerFilter)
{
    QVector<Candidate> result;
    for (int start = 0; start < segments.size(); ++start) {
        const QVariantMap first = segments.at(start).toMap();
        const QString firstSpeaker = first.value(QStringLiteral("speakerId")).toString();
        if (!speakerFilter.isEmpty() && firstSpeaker != speakerFilter) continue;
        const qint64 windowStart = first.value(QStringLiteral("startMs")).toLongLong();
        qint64 windowEnd = windowStart;
        QStringList textParts;

        for (int end = start; end < segments.size(); ++end) {
            const QVariantMap segment = segments.at(end).toMap();
            const QString segmentSpeaker = segment.value(QStringLiteral("speakerId")).toString();
            const qint64 segmentStart = segment.value(QStringLiteral("startMs")).toLongLong();
            const qint64 segmentEnd = segment.value(QStringLiteral("endMs")).toLongLong();
            if ((!speakerFilter.isEmpty() && segmentSpeaker != speakerFilter)
                || (!firstSpeaker.isEmpty() && segmentSpeaker != firstSpeaker)
                || segmentEnd <= segmentStart
                || (end > start && segmentStart - windowEnd > MaximumJoinGapMs)) {
                break;
            }
            if (segmentEnd - windowStart > MaximumReferenceMs) break;
            windowEnd = segmentEnd;
            const QString sourceText = segment.value(QStringLiteral("sourceText")).toString().trimmed();
            if (!sourceText.isEmpty()) textParts.append(sourceText);
            if (windowEnd - windowStart >= MinimumReferenceMs && !textParts.isEmpty()) {
                result.append(Candidate{windowStart, windowEnd, textParts.join(QLatin1Char(' ')),
                                        firstSpeaker, 0.0});
            }
        }
    }
    return result;
}

} // namespace

DubbingVoiceReference DubbingVoiceReferenceSelector::select(
    const QString &sourceAudioPath, const QVariantList &segments,
    const QString &projectPath, const QString &speakerId)
{
    DubbingVoiceReference result;
    if (sourceAudioPath.isEmpty() || !QFileInfo::exists(sourceAudioPath)) {
        result.error = QStringLiteral("Source speech audio is unavailable for automatic voice reference selection.");
        return result;
    }
    if (segments.isEmpty()) {
        result.error = QStringLiteral("Transcribe the source before selecting a voice reference.");
        return result;
    }

    QString decodeError;
    const WavIO::WavData audio = AudioFileDecoder::decodeMono(sourceAudioPath, 24000, &decodeError);
    if (audio.samples.isEmpty() || audio.sampleRate <= 0) {
        result.error = decodeError.isEmpty()
            ? QStringLiteral("Could not decode source speech for voice reference selection.")
            : decodeError;
        return result;
    }

    QVector<Candidate> candidates = buildCandidates(segments, speakerId);
    if (candidates.isEmpty()) {
        result.error = QStringLiteral("No continuous 3-15 second speech segment is available for voice cloning.");
        return result;
    }

    const qint64 audioFrames = audio.samples.size();
    Candidate best;
    bool found = false;
    for (Candidate candidate : candidates) {
        const qint64 begin = qBound<qint64>(0, candidate.startMs * audio.sampleRate / 1000, audioFrames);
        const qint64 end = qBound<qint64>(begin, candidate.endMs * audio.sampleRate / 1000, audioFrames);
        if (end - begin < audio.sampleRate * MinimumReferenceMs / 1000) continue;
        const QVector<float> window = audio.samples.mid(static_cast<int>(begin), static_cast<int>(end - begin));
        candidate.score = signalQuality(window, audio.sampleRate);
        if (!found || candidate.score > best.score
            || (qFuzzyCompare(candidate.score, best.score)
                && candidate.endMs - candidate.startMs > best.endMs - best.startMs)) {
            best = candidate;
            found = true;
        }
    }
    if (!found) {
        result.error = QStringLiteral("Source speech is too short for a reliable voice reference.");
        return result;
    }

    const qint64 begin = best.startMs * audio.sampleRate / 1000;
    const qint64 end = qMin<qint64>(audio.samples.size(), best.endMs * audio.sampleRate / 1000);
    const QVector<float> selected = audio.samples.mid(static_cast<int>(begin), static_cast<int>(end - begin));
    const QString projectDirectory = QFileInfo(projectPath).absolutePath();
    QDir referenceDirectory(QDir(projectDirectory).filePath(QStringLiteral(".workflow-artifacts/voice-references")));
    if (!referenceDirectory.mkpath(QStringLiteral("."))) {
        result.error = QStringLiteral("Could not create the voice reference cache directory.");
        return result;
    }

    const QByteArray fingerprint = QCryptographicHash::hash(
        QStringLiteral("%1|%2|%3|%4")
            .arg(QFileInfo(sourceAudioPath).absoluteFilePath())
            .arg(best.startMs).arg(best.endMs).arg(QFileInfo(sourceAudioPath).lastModified().toMSecsSinceEpoch())
            .toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    const QString outputPath = referenceDirectory.filePath(
        QStringLiteral("auto-reference-%1.wav").arg(QString::fromLatin1(fingerprint)));
    if (!QFileInfo::exists(outputPath)
        && !WavIO::saveFloat(outputPath, selected.constData(), selected.size(), audio.sampleRate)) {
        result.error = QStringLiteral("Could not save the selected voice reference.");
        return result;
    }

    result.audioPath = outputPath;
    result.referenceText = best.text;
    result.speakerId = best.speakerId;
    result.startMs = best.startMs;
    result.endMs = best.endMs;
    result.qualityScore = best.score;
    return result;
}

} // namespace LAStudio
