#include "dubbing/DubbingTimingService.h"

#include "audio/AudioTimelineRenderer.h"

#include <QFileInfo>
#include <QtMath>

namespace LAStudio {

QVariantList DubbingTimingService::fitSegments(const QVariantList &segments,
                                                QAtomicInteger<bool> *cancel,
                                                QString *error)
{
    const auto cancelled = [cancel]() { return cancel && cancel->loadAcquire(); };
    QVariantList result = segments;
    const double maxRate = 1.12;
    for (int i = 0; i < result.size(); ++i) {
        if (cancelled()) {
            if (error) *error = QStringLiteral("Timing fit cancelled.");
            return {};
        }
        QVariantMap segment = result.at(i).toMap();
        if (!segment.value(QStringLiteral("timingConflict")).toBool()
            && (segment.value(QStringLiteral("fitMethod")).toString() == QStringLiteral("atempo")
                || segment.value(QStringLiteral("fitMethod")).toString() == QStringLiteral("natural-with-padding")))
            continue;

        const QString input = segment.value(QStringLiteral("clipPath")).toString();
        if (input.isEmpty() || !QFileInfo::exists(input)) {
            result[i] = segment;
            continue;
        }
        const qint64 slotMs = qMax<qint64>(1, segment.value(QStringLiteral("endMs")).toLongLong()
                                              - segment.value(QStringLiteral("startMs")).toLongLong());
        const qint64 naturalMs = qMax<qint64>(1, segment.value(QStringLiteral("sourceDurationMs")).toLongLong());
        const double fitFactor = double(naturalMs) / double(slotMs);
        if (segment.value(QStringLiteral("durationMetric")).toString()
            == QStringLiteral("phoneme-distance")) {
            const bool conflict = fitFactor < 0.80 || fitFactor > 1.20;
            segment.insert(QStringLiteral("fitFactor"), fitFactor);
            segment.insert(QStringLiteral("timingConflict"), conflict);
            segment.insert(QStringLiteral("fitMethod"), QStringLiteral("paper-dt-natural"));
            segment.insert(QStringLiteral("state"), conflict ? QStringLiteral("conflict") : QStringLiteral("ready"));
            result[i] = segment;
            continue;
        }
        if (fitFactor > maxRate) {
            segment.insert(QStringLiteral("timingConflict"), true);
            segment.insert(QStringLiteral("fitMethod"), QStringLiteral("none"));
            segment.insert(QStringLiteral("state"), QStringLiteral("conflict"));
            result[i] = segment;
            continue;
        }
        if (fitFactor > 1.0) {
            const QString output = input + QStringLiteral(".fitted.wav");
            AudioRenderResult renderResult;
            QString renderError;
            if (!AudioTimelineRenderer::renderClip(
                    input, output, segment.value(QStringLiteral("sampleRate")).toInt(),
                    qMax(1, qRound64(slotMs * segment.value(QStringLiteral("sampleRate")).toInt() / 1000.0)),
                    fitFactor, &renderResult, &renderError)) {
                segment.insert(QStringLiteral("timingConflict"), true);
                segment.insert(QStringLiteral("fitMethod"), QStringLiteral("failed"));
                segment.insert(QStringLiteral("fitError"), renderError);
                segment.insert(QStringLiteral("state"), QStringLiteral("conflict"));
            } else {
                segment.insert(QStringLiteral("clipPath"), output);
                segment.insert(QStringLiteral("durationMs"), slotMs);
                segment.insert(QStringLiteral("sampleCount"),
                               qRound64(slotMs * segment.value(QStringLiteral("sampleRate")).toInt() / 1000.0));
                segment.insert(QStringLiteral("timingConflict"), false);
                segment.insert(QStringLiteral("fitMethod"), renderResult.usedFallback
                               ? QStringLiteral("linear-fallback") : QStringLiteral("atempo"));
                segment.insert(QStringLiteral("state"), QStringLiteral("ready"));
            }
        } else {
            segment.insert(QStringLiteral("timingConflict"), false);
            segment.insert(QStringLiteral("fitMethod"), QStringLiteral("natural-with-padding"));
            segment.insert(QStringLiteral("state"), QStringLiteral("ready"));
        }
        segment.insert(QStringLiteral("fitFactor"), fitFactor);
        result[i] = segment;
    }
    return result;
}

} // namespace LAStudio
