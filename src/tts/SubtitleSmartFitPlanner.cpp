#include "SubtitleSmartFitPlanner.h"

#include <QtGlobal>

namespace LAStudio {

QVector<SubtitleFit> SubtitleSmartFitPlanner::plan(
    const QVector<TimedTextCue> &cues, const QVector<qint64> &naturalDurationsMs)
{
    QVector<SubtitleFit> result;
    if (cues.size() != naturalDurationsMs.size()) return result;
    result.reserve(cues.size());

    qint64 lastScheduledEnd = 0;
    for (int i = 0; i < cues.size(); ++i) {
        const TimedTextCue &cue = cues.at(i);
        SubtitleFit fit;
        fit.scheduledStartMs = qMax(cue.startMs, lastScheduledEnd);
        if (cue.endMs <= fit.scheduledStartMs) {
            fit.droppedOverlap = true;
            fit.effectiveEndMs = fit.scheduledStartMs;
            fit.slotMs = 0;
            fit.outputMs = 0;
            fit.status = QStringLiteral("dropped_overlap");
            result.append(fit);
            continue;
        }

        fit.effectiveEndMs = cue.endMs;
        if (i + 1 < cues.size()) {
            const qint64 nextStart = qMax(cues.at(i + 1).startMs, cue.endMs);
            if (nextStart > cue.endMs)
                fit.effectiveEndMs = qMax(fit.effectiveEndMs, nextStart - 50);
        }
        fit.slotMs = qMax<qint64>(1, fit.effectiveEndMs - fit.scheduledStartMs);
        const qint64 natural = qMax<qint64>(0, naturalDurationsMs.at(i));
        const double need = natural > 0 ? double(natural) / double(fit.slotMs) : 0.0;
        if (need <= 1.0) {
            if (need > 0.0 && need < 0.95) {
                fit.audioRate = qMax(need, 0.85);
                fit.status = QStringLiteral("audio_slowed");
            }
        } else if (need <= 1.20) {
            fit.audioRate = need;
            fit.status = QStringLiteral("audio_stretched");
        } else {
            fit.audioRate = qMin(need, 1.80);
            fit.status = need > 1.80 ? QStringLiteral("overflow_trimmed")
                                     : QStringLiteral("audio_stretched");
        }
        const qint64 stretched = fit.audioRate > 0.0
            ? qRound64(double(natural) / fit.audioRate) : natural;
        fit.outputMs = qMin(fit.slotMs, qMax<qint64>(0, stretched));
        fit.overflowMs = qMax<qint64>(0, stretched - fit.slotMs);
        if (fit.overflowMs > 0) fit.status = QStringLiteral("overflow_trimmed");
        lastScheduledEnd = cue.endMs;
        result.append(fit);
    }
    return result;
}

} // namespace LAStudio
