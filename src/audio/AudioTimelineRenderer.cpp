#include "AudioTimelineRenderer.h"

#include "audio/WavIO.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QtMath>

namespace LAStudio {
namespace {

QVector<float> resampleToCount(const QVector<float> &source, int targetCount)
{
    if (source.isEmpty() || targetCount <= 0 || source.size() == targetCount) return source;
    QVector<float> result(targetCount);
    const double ratio = double(source.size() - 1) / qMax(1, targetCount - 1);
    for (int i = 0; i < targetCount; ++i) {
        const double sourceIndex = i * ratio;
        const int left = qBound(0, static_cast<int>(sourceIndex), source.size() - 1);
        const int right = qMin(left + 1, source.size() - 1);
        const float fraction = float(sourceIndex - left);
        result[i] = source.at(left) * (1.0f - fraction) + source.at(right) * fraction;
    }
    return result;
}

} // namespace

bool AudioTimelineRenderer::renderClip(const QString &inputPath, const QString &outputPath,
                                       int sampleRate, int targetSamples, double rate,
                                       AudioRenderResult *result, QString *error)
{
    if (result) *result = {};
    if (inputPath.isEmpty() || outputPath.isEmpty() || sampleRate <= 0 || targetSamples <= 0) {
        if (error) *error = QStringLiteral("Invalid audio fitting request.");
        return false;
    }

    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    const QString processedPath = outputPath + QStringLiteral(".atempo.wav");
    bool ffmpegOk = false;
    if (!ffmpeg.isEmpty()) {
        QProcess process;
        process.start(ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                               QStringLiteral("-i"), inputPath, QStringLiteral("-filter:a"),
                               QStringLiteral("atempo=%1").arg(rate, 0, 'f', 6),
                               QStringLiteral("-ar"), QString::number(sampleRate), QStringLiteral("-ac"), QStringLiteral("1"),
                               QStringLiteral("-c:a"), QStringLiteral("pcm_s16le"), processedPath});
        ffmpegOk = process.waitForStarted(5000) && process.waitForFinished(-1)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    WavIO::WavData wav = ffmpegOk ? WavIO::loadAsFloat(processedPath) : WavIO::WavData();
    if (!ffmpegOk || wav.samples.isEmpty()) {
        if (result) result->usedFallback = true;
        wav = WavIO::loadAsFloat(inputPath);
        if (wav.samples.isEmpty()) {
            if (error) *error = QStringLiteral("Could not read generated audio for fitting.");
            return false;
        }
        wav.samples = resampleToCount(wav.samples, targetSamples);
    } else if (wav.samples.size() != targetSamples) {
        wav.samples = resampleToCount(wav.samples, targetSamples);
    }

    const int fade = qMin(targetSamples / 2, qMax(1, qRound(sampleRate * 0.015)));
    for (int i = 0; i < fade; ++i) {
        const float gain = float(i) / float(qMax(1, fade));
        wav.samples[i] *= gain;
        wav.samples[targetSamples - 1 - i] *= gain;
    }
    QFile::remove(outputPath);
    if (!WavIO::saveFloat(outputPath, wav.samples.constData(), wav.samples.size(), sampleRate)) {
        if (error) *error = QStringLiteral("Failed to write fitted audio.");
        return false;
    }
    return true;
}

bool AudioTimelineRenderer::assemble(const QVector<QString> &clipPaths,
                                     const QVector<AudioTimelinePlacement> &placements,
                                     const QString &outputPath, int sampleRate, QString *error)
{
    if (clipPaths.size() != placements.size()
        || outputPath.isEmpty() || sampleRate <= 0) {
        if (error) *error = QStringLiteral("Invalid audio timeline assembly request.");
        return false;
    }
    qint64 durationMs = 0;
    for (const AudioTimelinePlacement &placement : placements)
        durationMs = qMax(durationMs, placement.endMs);
    const qint64 totalSamples = qRound64(durationMs * sampleRate / 1000.0);
    if (totalSamples <= 0 || totalSamples > 48000LL * 60LL * 60LL) {
        if (error) *error = QStringLiteral("Subtitle timeline is too long to assemble safely.");
        return false;
    }

    QVector<float> mix(static_cast<int>(totalSamples), 0.0f);
    for (int i = 0; i < clipPaths.size(); ++i) {
        if (!placements.at(i).enabled || clipPaths.at(i).isEmpty()) continue;
        const WavIO::WavData wav = WavIO::loadAsFloat(clipPaths.at(i));
        if (wav.samples.isEmpty()) continue;
        const qint64 start = qRound64(placements.at(i).startMs * sampleRate / 1000.0);
        for (int j = 0; j < wav.samples.size() && start + j < mix.size(); ++j)
            mix[static_cast<int>(start + j)] = wav.samples.at(j);
    }
    if (!WavIO::saveFloat(outputPath, mix.constData(), mix.size(), sampleRate)) {
        if (error) *error = QStringLiteral("Failed to write final subtitle voice WAV.");
        return false;
    }
    return true;
}

} // namespace LAStudio
