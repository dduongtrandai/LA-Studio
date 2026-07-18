#include "dubbing/AudioTimelineMixer.h"

#include "audio/WavIO.h"
#include <QFileInfo>
#include <QDir>
#include <QtMath>

namespace LAStudio {

QVector<float> AudioTimelineMixer::resampleToCount(const QVector<float> &source, int targetCount)
{
    if (source.isEmpty() || targetCount <= 0 || source.size() == targetCount) return source;
    QVector<float> result(targetCount);
    const double ratio = static_cast<double>(source.size() - 1) / qMax(1, targetCount - 1);
    for (int i = 0; i < targetCount; ++i) {
        const double sourceIndex = i * ratio;
        const int left = qBound(0, static_cast<int>(sourceIndex), source.size() - 1);
        const int right = qMin(left + 1, source.size() - 1);
        const float fraction = static_cast<float>(sourceIndex - left);
        result[i] = source.at(left) * (1.0f - fraction) + source.at(right) * fraction;
    }
    return result;
}

bool AudioTimelineMixer::mixSegments(const QVariantList &segments, const QString &outputPath, QString *error)
{
    return mixSegments(segments, outputPath, QString(), error);
}

bool AudioTimelineMixer::mixSegments(const QVariantList &segments, const QString &outputPath,
                                     const QString &backgroundPath, QString *error)
{
    if (segments.isEmpty()) {
        if (error) *error = QStringLiteral("There are no dubbing segments to render.");
        return false;
    }

    constexpr int outputRate = 48000;
    qint64 outputSamples = 0;
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
        const QString clipPath = segment.value(QStringLiteral("clipPath")).toString();
        if (clipPath.isEmpty() || !QFileInfo::exists(clipPath)) continue;
        const qint64 start = qMax<qint64>(0, segment.value(QStringLiteral("startMs")).toLongLong());
        const qint64 end = qMax(start, segment.value(QStringLiteral("endMs")).toLongLong());
        outputSamples = qMax(outputSamples, qMax(end * outputRate / 1000, start * outputRate / 1000));
    }

    if (outputSamples <= 0 || outputSamples > 48000LL * 60LL * 60LL) {
        if (error) *error = QStringLiteral("No generated clips are available, or the render duration is invalid.");
        return false;
    }

    QVector<float> mix(static_cast<int>(outputSamples), 0.0f);
    if (!backgroundPath.isEmpty() && QFileInfo::exists(backgroundPath)) {
        const WavIO::WavData background = WavIO::loadAsFloat(backgroundPath);
        if (!background.samples.isEmpty() && background.sampleRate > 0) {
            for (int i = 0; i < mix.size(); ++i) {
                const int source = qMin(background.samples.size() - 1,
                                        static_cast<int>(static_cast<double>(i) * background.sampleRate / outputRate));
                mix[i] = background.samples.at(source) * 0.35f;
            }
        }
    }
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
        const QString clipPath = segment.value(QStringLiteral("clipPath")).toString();
        if (clipPath.isEmpty() || !QFileInfo::exists(clipPath)) continue;
        const WavIO::WavData clip = WavIO::loadAsFloat(clipPath);
        if (clip.samples.isEmpty() || clip.sampleRate <= 0) continue;
        const qint64 startSample = qMax<qint64>(0, segment.value(QStringLiteral("startMs")).toLongLong() * outputRate / 1000);
        for (int i = 0; i < clip.samples.size(); ++i) {
            const qint64 destination = startSample + static_cast<qint64>(i) * outputRate / clip.sampleRate;
            if (destination >= 0 && destination < mix.size()) {
                mix[static_cast<int>(destination)] = qBound(-1.0f, mix[static_cast<int>(destination)] + clip.samples.at(i), 1.0f);
            }
        }
    }

    if (!WavIO::saveFloat(outputPath, mix.constData(), mix.size(), outputRate)) {
        if (error) *error = QStringLiteral("Failed to render preview WAV: %1").arg(outputPath);
        return false;
    }

    return true;
}

} // namespace LAStudio
