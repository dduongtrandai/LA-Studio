#include "DubbingSegmentNormalizer.h"

#include <QRegularExpression>
#include <QUuid>
#include <QtMath>

namespace LAStudio {
namespace {

constexpr qint64 kMinSegmentMs = 1000;
constexpr qint64 kMaxSegmentMs = 12000;
constexpr qint64 kPauseBreakMs = 650;
constexpr int kMaxWords = 24;

QString wordText(const QVariantMap &word)
{
    return word.value(QStringLiteral("text"), word.value(QStringLiteral("word"))).toString().trimmed();
}

QString joinWords(const QVariantList &words)
{
    QStringList parts;
    parts.reserve(words.size());
    for (const QVariant &entry : words) {
        const QString text = wordText(entry.toMap());
        if (!text.isEmpty()) parts.append(text);
    }
    QString text = parts.join(QLatin1Char(' '));
    text.replace(QRegularExpression(QStringLiteral("\\s+([,.;:!?%])")), QStringLiteral("\\1"));
    text.replace(QRegularExpression(QStringLiteral("([([{])\\s+")), QStringLiteral("\\1"));
    return text.trimmed();
}

bool sentenceEnd(const QString &text)
{
    return text.endsWith(QLatin1Char('.')) || text.endsWith(QLatin1Char('!'))
        || text.endsWith(QLatin1Char('?')) || text.endsWith(QLatin1Char(';'))
        || text.endsWith(QLatin1Char(':'));
}

QVariantMap childSegment(const QVariantMap &source, const QString &parentId)
{
    QVariantMap child = source;
    child.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!parentId.isEmpty()) child.insert(QStringLiteral("derivedFromSegmentId"), parentId);
    child.insert(QStringLiteral("targetText"), QString());
    child.insert(QStringLiteral("state"), QStringLiteral("transcribed"));
    return child;
}

QVariantList splitUsingWords(const QVariantMap &segment)
{
    const QVariantList words = segment.value(QStringLiteral("words")).toList();
    if (words.size() < 2) return {segment};

    QVector<QVariantList> chunks;
    QVariantList current;
    qint64 chunkStart = -1;
    for (int i = 0; i < words.size(); ++i) {
        const QVariantMap word = words.at(i).toMap();
        const qint64 start = word.value(QStringLiteral("startMs")).toLongLong();
        const qint64 end = word.value(QStringLiteral("endMs")).toLongLong();
        if (end <= start) return {segment};
        if (current.isEmpty()) chunkStart = start;
        current.append(word);

        const qint64 duration = end - chunkStart;
        const qint64 nextGap = i + 1 < words.size()
            ? words.at(i + 1).toMap().value(QStringLiteral("startMs")).toLongLong() - end : 0;
        const bool softBoundary = sentenceEnd(wordText(word)) || nextGap >= kPauseBreakMs;
        const bool hardBoundary = duration >= kMaxSegmentMs || current.size() >= kMaxWords;
        if (i + 1 < words.size() && (hardBoundary || (softBoundary && duration >= kMinSegmentMs))) {
            chunks.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty()) chunks.append(current);
    if (chunks.size() <= 1) return {segment};

    if (chunks.size() >= 2) {
        const QVariantList &tail = chunks.constLast();
        const qint64 tailStart = tail.constFirst().toMap().value(QStringLiteral("startMs")).toLongLong();
        const qint64 tailEnd = tail.constLast().toMap().value(QStringLiteral("endMs")).toLongLong();
        QVariantList &previous = chunks[chunks.size() - 2];
        const qint64 previousStart = previous.constFirst().toMap().value(QStringLiteral("startMs")).toLongLong();
        if (tailEnd - tailStart < kMinSegmentMs && tailEnd - previousStart <= kMaxSegmentMs) {
            previous.append(tail);
            chunks.removeLast();
        }
    }

    QVariantList result;
    const QString parentId = segment.value(QStringLiteral("id")).toString();
    const QStringList sourceWords = segment.value(QStringLiteral("sourceText")).toString()
        .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const bool canPreserveSourceText = sourceWords.size() == words.size();
    int sourceOffset = 0;
    for (const QVariantList &chunk : chunks) {
        QVariantMap child = childSegment(segment, parentId);
        child.insert(QStringLiteral("startMs"), chunk.constFirst().toMap().value(QStringLiteral("startMs")));
        child.insert(QStringLiteral("endMs"), chunk.constLast().toMap().value(QStringLiteral("endMs")));
        child.insert(QStringLiteral("sourceText"), canPreserveSourceText
                     ? sourceWords.mid(sourceOffset, chunk.size()).join(QLatin1Char(' '))
                     : joinWords(chunk));
        child.insert(QStringLiteral("words"), chunk);
        result.append(child);
        sourceOffset += chunk.size();
    }
    return result;
}

struct TextChunk
{
    QStringList words;
};

QVariantList splitUsingText(const QVariantMap &segment)
{
    const QStringList words = segment.value(QStringLiteral("sourceText")).toString()
        .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    const qint64 startMs = segment.value(QStringLiteral("startMs")).toLongLong();
    const qint64 endMs = segment.value(QStringLiteral("endMs")).toLongLong();
    const qint64 duration = endMs - startMs;
    if (words.size() < 2 || duration <= kMaxSegmentMs) return {segment};

    const double msPerWord = double(duration) / double(words.size());
    const int durationBound = qMax(4, int(qFloor(double(kMaxSegmentMs) / msPerWord)));
    const int maxWords = qMin(kMaxWords, durationBound);
    QVector<TextChunk> chunks;
    TextChunk current;
    for (int i = 0; i < words.size(); ++i) {
        current.words.append(words.at(i));
        const qint64 estimatedDuration = qRound64(current.words.size() * msPerWord);
        const bool softBoundary = sentenceEnd(words.at(i));
        const bool hardBoundary = current.words.size() >= maxWords || estimatedDuration >= kMaxSegmentMs;
        if (i + 1 < words.size() && (hardBoundary || (softBoundary && estimatedDuration >= kMinSegmentMs))) {
            chunks.append(current);
            current.words.clear();
        }
    }
    if (!current.words.isEmpty()) chunks.append(current);
    if (chunks.size() <= 1) return {segment};

    if (chunks.size() >= 2 && qRound64(chunks.constLast().words.size() * msPerWord) < kMinSegmentMs) {
        chunks[chunks.size() - 2].words.append(chunks.constLast().words);
        chunks.removeLast();
    }

    QVariantList result;
    const QString parentId = segment.value(QStringLiteral("id")).toString();
    int consumedWords = 0;
    for (int i = 0; i < chunks.size(); ++i) {
        const TextChunk &chunk = chunks.at(i);
        const qint64 childStart = startMs + qRound64(double(consumedWords) / words.size() * duration);
        consumedWords += chunk.words.size();
        const qint64 childEnd = i + 1 == chunks.size()
            ? endMs : startMs + qRound64(double(consumedWords) / words.size() * duration);
        QVariantMap child = childSegment(segment, parentId);
        child.remove(QStringLiteral("words"));
        child.insert(QStringLiteral("startMs"), childStart);
        child.insert(QStringLiteral("endMs"), childEnd);
        child.insert(QStringLiteral("sourceText"), chunk.words.join(QLatin1Char(' ')));
        child.insert(QStringLiteral("timingSource"), QStringLiteral("asr-interpolated"));
        child.insert(QStringLiteral("alignmentDiagnostic"),
                     QStringLiteral("Segment timing was interpolated because word alignment was unavailable."));
        result.append(child);
    }
    return result;
}

} // namespace

QVariantList DubbingSegmentNormalizer::normalize(const QVariantList &segments)
{
    QVariantList result;
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
        const QVariantList split = segment.value(QStringLiteral("words")).toList().isEmpty()
            ? splitUsingText(segment) : splitUsingWords(segment);
        result.append(split);
    }
    return result;
}

} // namespace LAStudio
