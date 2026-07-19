#include "dubbing/DubbingDuration.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>

namespace LAStudio {

QVariantMap DubbingDurationSettings::toVariantMap() const
{
    return {{QStringLiteral("enabled"), enabled},
            {QStringLiteral("unit"), unit},
            {QStringLiteral("safetyFactor"), safetyFactor},
            {QStringLiteral("toleranceRatio"), toleranceRatio},
            {QStringLiteral("pauseThresholdMs"), pauseThresholdMs},
            {QStringLiteral("maxPreTtsIterations"), maxPreTtsIterations},
            {QStringLiteral("candidatesPerIteration"), candidatesPerIteration},
            {QStringLiteral("maxPostTtsRepairs"), maxPostTtsRepairs},
            {QStringLiteral("maxFitRate"), maxFitRate}};
}

DubbingDurationSettings DubbingDurationSettings::fromVariantMap(const QVariantMap &map)
{
    DubbingDurationSettings result;
    if (map.contains(QStringLiteral("enabled")))
        result.enabled = map.value(QStringLiteral("enabled")).toBool();
    result.unit = map.value(QStringLiteral("unit"), result.unit).toString();
    if (result.unit == QStringLiteral("vi-syllable-v1"))
        result.unit = QStringLiteral("phoneme-v1");
    result.safetyFactor = qBound(
        0.50, map.value(QStringLiteral("safetyFactor"), result.safetyFactor).toDouble(), 1.0);
    result.toleranceRatio = qBound(
        0.01, map.value(QStringLiteral("toleranceRatio"), result.toleranceRatio).toDouble(), 0.50);
    result.pauseThresholdMs = qBound(
        80, map.value(QStringLiteral("pauseThresholdMs"), result.pauseThresholdMs).toInt(), 2000);
    result.maxPreTtsIterations = qBound(
        0, map.value(QStringLiteral("maxPreTtsIterations"), result.maxPreTtsIterations).toInt(), 8);
    result.candidatesPerIteration = qBound(
        1, map.value(QStringLiteral("candidatesPerIteration"), result.candidatesPerIteration).toInt(), 5);
    result.maxPostTtsRepairs = qBound(
        0, map.value(QStringLiteral("maxPostTtsRepairs"), result.maxPostTtsRepairs).toInt(), 3);
    result.maxFitRate = qBound(
        1.0, map.value(QStringLiteral("maxFitRate"), result.maxFitRate).toDouble(), 1.5);
    return result;
}

QVariantMap DubbingPause::toVariantMap() const
{
    return {{QStringLiteral("offsetMs"), offsetMs},
            {QStringLiteral("durationMs"), durationMs},
            {QStringLiteral("kind"), kind}};
}

QVariantMap DubbingSpeechBudget::toVariantMap() const
{
    return {{QStringLiteral("slotMs"), slotMs},
            {QStringLiteral("speechWindowMs"), speechWindowMs},
            {QStringLiteral("phonemesPerSecond"), phonemesPerSecond},
            {QStringLiteral("targetUnits"), targetUnits},
            {QStringLiteral("minUnits"), minUnits},
            {QStringLiteral("maxUnits"), maxUnits},
            {QStringLiteral("confidence"), confidence},
            {QStringLiteral("pauses"), pauses}};
}

QVariantList DubbingDurationPlanner::extractPauses(const QVariantMap &segment, int thresholdMs)
{
    const QVariantList words = segment.value(QStringLiteral("words")).toList();
    QVariantList result;
    if (words.isEmpty()) return result;

    const qint64 segmentStart = segment.value(QStringLiteral("startMs")).toLongLong();
    const qint64 segmentEnd = segment.value(QStringLiteral("endMs")).toLongLong();
    qint64 previousEnd = 0;
    bool havePrevious = false;
    for (const QVariant &value : words) {
        const QVariantMap word = value.toMap();
        const qint64 start = word.value(QStringLiteral("startMs")).toLongLong();
        const qint64 end = word.value(QStringLiteral("endMs")).toLongLong();
        if (end <= start) continue;
        if (!havePrevious) {
            const qint64 leading = qMax<qint64>(0, start - segmentStart);
            if (leading >= thresholdMs) {
                result.append(QVariantMap{
                    {QStringLiteral("offsetMs"), 0},
                    {QStringLiteral("durationMs"), qMin<qint64>(1500, leading)},
                    {QStringLiteral("kind"), QStringLiteral("leading")}});
            }
            havePrevious = true;
        } else {
            const qint64 gap = start - previousEnd;
            if (gap >= thresholdMs) {
                result.append(QVariantMap{
                    {QStringLiteral("offsetMs"), previousEnd - segmentStart},
                    {QStringLiteral("durationMs"), qMin<qint64>(1500, gap)},
                    {QStringLiteral("kind"), QStringLiteral("internal")}});
            }
        }
        previousEnd = qMax(previousEnd, end);
    }
    if (havePrevious) {
        const qint64 trailing = qMax<qint64>(0, segmentEnd - previousEnd);
        if (trailing >= thresholdMs) {
            result.append(QVariantMap{
                {QStringLiteral("offsetMs"), previousEnd - segmentStart},
                {QStringLiteral("durationMs"), qMin<qint64>(1500, trailing)},
                {QStringLiteral("kind"), QStringLiteral("trailing")}});
        }
    }
    return result;
}

QString DubbingDurationPlanner::normalizeSpokenVietnamese(const QString &text)
{
    QString normalized = text.normalized(QString::NormalizationForm_C).simplified();
    normalized.replace(QRegularExpression(QStringLiteral("[‘’]")), QStringLiteral("'"));
    normalized.replace(QRegularExpression(QStringLiteral("[“”]")), QStringLiteral("\""));
    return normalized;
}

int DubbingDurationPlanner::countVietnameseSyllables(const QString &text)
{
    const QString normalized = normalizeSpokenVietnamese(text);
    if (normalized.isEmpty()) return 0;
    int count = 0;
    const QStringList tokens = normalized.split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token.remove(QRegularExpression(
            QStringLiteral("^[^\\p{L}\\p{N}]+|[^\\p{L}\\p{N}]+$")));
        if (!token.isEmpty()) ++count;
    }
    return count;
}

int DubbingDurationPlanner::countPhonemes(const QString &text, const QString &language)
{
    QString normalized = normalizeSpokenVietnamese(text)
                             .normalized(QString::NormalizationForm_D)
                             .toLower();
    normalized.remove(QRegularExpression(QStringLiteral("\\p{M}+")));
    if (normalized.isEmpty()) return 0;

    if (language.compare(QStringLiteral("vi"), Qt::CaseInsensitive) == 0) {
        static const QStringList onsets = {
            QStringLiteral("ngh"), QStringLiteral("ch"), QStringLiteral("gh"),
            QStringLiteral("gi"), QStringLiteral("kh"), QStringLiteral("ng"),
            QStringLiteral("nh"), QStringLiteral("ph"), QStringLiteral("qu"),
            QStringLiteral("th"), QStringLiteral("tr")
        };
        static const QStringList codas = {
            QStringLiteral("ch"), QStringLiteral("ng"), QStringLiteral("nh"),
            QStringLiteral("c"), QStringLiteral("m"), QStringLiteral("n"),
            QStringLiteral("p"), QStringLiteral("t")
        };
        static const QSet<QChar> vowels = {
            u'a', u'ă', u'â', u'e', u'ê', u'i', u'o', u'ô', u'ơ', u'u', u'ư', u'y'
        };
        int total = 0;
        const QStringList words = normalized.split(
            QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), Qt::SkipEmptyParts);
        for (const QString &word : words) {
            int onsetLength = 0;
            for (const QString &onset : onsets) {
                if (word.startsWith(onset) && onset.size() > onsetLength)
                    onsetLength = onset.size();
            }
            if (onsetLength == 0 && !word.isEmpty() && !vowels.contains(word.front()))
                onsetLength = 1;

            int codaLength = 0;
            for (const QString &coda : codas) {
                if (word.endsWith(coda) && coda.size() > codaLength
                    && coda.size() < word.size() - onsetLength + 1) {
                    codaLength = coda.size();
                }
            }

            int wordUnits = onsetLength > 0 ? 1 : 0;
            const int nucleusEnd = word.size() - codaLength;
            for (int i = onsetLength; i < nucleusEnd; ++i) {
                if (vowels.contains(word.at(i))) ++wordUnits;
            }
            if (codaLength > 0) ++wordUnits;
            total += qMax(1, wordUnits);
        }
        return total;
    }

    int total = 0;
    const QStringList words = normalized.split(
        QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]+")), Qt::SkipEmptyParts);
    for (const QString &word : words) {
        bool previousWasVowel = false;
        for (const QChar ch : word) {
            const bool vowel = QStringLiteral("aeiouy").contains(ch);
            if (vowel) {
                if (!previousWasVowel) ++total;
            } else if (ch.isLetter()) {
                ++total;
            }
            previousWasVowel = vowel;
        }
    }
    return total;
}

int DubbingDurationPlanner::phonemeDistance(const QString &text, int predictedPhonemes,
                                            const QString &language)
{
    return qAbs(countPhonemes(text, language) - predictedPhonemes);
}

double DubbingDurationPlanner::semanticFidelityScore(const QString &reference,
                                                     const QString &candidate)
{
    auto trigrams = [](const QString &input) {
        QHash<QString, int> result;
        QString value = input.normalized(QString::NormalizationForm_C).toLower().simplified();
        value.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N} ]")));
        if (value.size() < 3) {
            if (!value.isEmpty()) result.insert(value, 1);
            return result;
        }
        for (int i = 0; i + 2 < value.size(); ++i)
            ++result[value.mid(i, 3)];
        return result;
    };
    const QHash<QString, int> a = trigrams(reference);
    const QHash<QString, int> b = trigrams(candidate);
    if (a.isEmpty() || b.isEmpty()) return 0.0;
    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (auto it = a.cbegin(); it != a.cend(); ++it) {
        normA += it.value() * it.value();
        dot += it.value() * b.value(it.key());
    }
    for (auto it = b.cbegin(); it != b.cend(); ++it)
        normB += it.value() * it.value();
    return normA > 0.0 && normB > 0.0 ? dot / qSqrt(normA * normB) : 0.0;
}

QString DubbingDurationPlanner::selectBestCandidate(
    const QString &sourceText, const QString &reference, const QString &current,
    const QStringList &candidates, int predictedPhonemes,
    const QStringList &protectedTokens, const QString &language)
{
    Q_UNUSED(sourceText);
    const int currentDistance = phonemeDistance(current, predictedPhonemes, language);
    QString best;
    double bestScore = -1.0;
    int bestDistance = currentDistance;
    for (const QString &candidate : candidates) {
        const QString clean = candidate.trimmed();
        if (clean.isEmpty()) continue;
        const int distance = phonemeDistance(clean, predictedPhonemes, language);
        if (distance >= currentDistance) continue;
        bool preservesTokens = true;
        for (const QString &token : protectedTokens) {
            if (!clean.contains(token, Qt::CaseInsensitive)) {
                preservesTokens = false;
                break;
            }
        }
        if (!preservesTokens) continue;
        const double score = semanticFidelityScore(reference, clean);
        if (score > bestScore
            || (qFuzzyCompare(score + 1.0, bestScore + 1.0) && distance < bestDistance)) {
            best = clean;
            bestScore = score;
            bestDistance = distance;
        }
    }
    return best;
}

QString DubbingDurationPlanner::textWithoutPauseMarkers(const QString &markedText)
{
    QString result = markedText;
    result.remove(QRegularExpression(
        QStringLiteral("\\[\\[\\s*PAUSE\\s*\\]\\]"),
        QRegularExpression::CaseInsensitiveOption));
    return result.simplified();
}

QVariantList DubbingDurationPlanner::pauseChunks(const QString &markedText,
                                                 const QVariantList &sourcePauses)
{
    QVariantList internalPauses;
    qint64 leadingMs = 0;
    qint64 trailingMs = 0;
    for (const QVariant &value : sourcePauses) {
        const QVariantMap pause = value.toMap();
        const QString kind = pause.value(QStringLiteral("kind")).toString();
        if (kind == QStringLiteral("leading"))
            leadingMs = pause.value(QStringLiteral("durationMs")).toLongLong();
        else if (kind == QStringLiteral("trailing"))
            trailingMs = pause.value(QStringLiteral("durationMs")).toLongLong();
        else if (kind == QStringLiteral("internal"))
            internalPauses.append(pause);
    }

    QStringList parts = markedText.split(
        QRegularExpression(QStringLiteral("\\[\\[\\s*PAUSE\\s*\\]\\]"),
                           QRegularExpression::CaseInsensitiveOption),
        Qt::KeepEmptyParts);
    if (parts.size() == 1 && !internalPauses.isEmpty()) {
        const QStringList words = markedText.simplified().split(
            QLatin1Char(' '), Qt::SkipEmptyParts);
        if (words.size() >= internalPauses.size() + 1) {
            parts.clear();
            const int groupCount = internalPauses.size() + 1;
            for (int group = 0; group < groupCount; ++group) {
                const int begin = group * words.size() / groupCount;
                const int end = (group + 1) * words.size() / groupCount;
                parts.append(words.mid(begin, end - begin).join(QLatin1Char(' ')));
            }
        }
    }
    QVariantList chunks;
    int internalIndex = 0;
    for (const QString &part : parts) {
        const QString text = part.trimmed();
        if (text.isEmpty()) continue;
        qint64 pauseAfterMs = 0;
        if (internalIndex < internalPauses.size()) {
            pauseAfterMs = internalPauses.at(internalIndex)
                               .toMap().value(QStringLiteral("durationMs")).toLongLong();
            ++internalIndex;
        }
        chunks.append(QVariantMap{
            {QStringLiteral("text"), text},
            {QStringLiteral("pauseAfterMs"), pauseAfterMs},
            {QStringLiteral("leadingPauseMs"), chunks.isEmpty() ? leadingMs : 0}});
    }
    if (!chunks.isEmpty()) {
        QVariantMap last = chunks.last().toMap();
        last.insert(QStringLiteral("pauseAfterMs"), trailingMs);
        chunks.last() = last;
    }
    return chunks;
}

DubbingSpeechBudget DubbingDurationPlanner::plan(const QVariantMap &segment,
                                                 double phonemesPerSecond,
                                                 const DubbingDurationSettings &settings)
{
    DubbingSpeechBudget result;
    result.slotMs = qMax<qint64>(
        1, segment.value(QStringLiteral("endMs")).toLongLong()
               - segment.value(QStringLiteral("startMs")).toLongLong());
    result.pauses = extractPauses(segment, settings.pauseThresholdMs);
    qint64 pauseMs = 0;
    for (const QVariant &pause : result.pauses)
        pauseMs += pause.toMap().value(QStringLiteral("durationMs")).toLongLong();
    result.speechWindowMs = qMax<qint64>(
        400, result.slotMs - qMin(pauseMs, result.slotMs * 40 / 100));
    result.phonemesPerSecond = qMax(0.1, phonemesPerSecond);
    result.targetUnits = qMax(
        1, qRound(result.phonemesPerSecond * result.speechWindowMs / 1000.0
                  * settings.safetyFactor));
    const int delta = qMax(1, qRound(result.targetUnits * settings.toleranceRatio));
    result.minUnits = qMax(1, result.targetUnits - delta);
    result.maxUnits = result.targetUnits + delta;
    result.confidence = segment.value(QStringLiteral("words")).toList().isEmpty() ? 0.35 : 0.85;
    return result;
}

} // namespace LAStudio
