#include "dubbing/DubbingSegmentNormalizer.h"

#include <QRegularExpression>
#include <QUuid>
#include <QtMath>

namespace LAStudio {
namespace {

// Sentence-oriented thresholds adapted from OmniVoice Studio's dubbing
// segmenter. Subtitle presentation constraints belong at export time; these
// limits are deliberately generous enough to preserve translation context.
constexpr qint64 kMinSegmentMs = 1500;
constexpr qint64 kIdealSegmentMs = 4500;
constexpr qint64 kMaxSegmentMs = 9000;
constexpr qint64 kPauseBreakMs = 650;
constexpr qint64 kForcePauseMs = 350;
constexpr int kMinChars = 12;
constexpr int kMinWords = 3;
constexpr int kMaxChars = 140;

struct TimedWord
{
    QString text;
    qint64 startMs = 0;
    qint64 endMs = 0;
    double confidence = 0.0;
    QVariantMap source;
    QString parentId;
    QString speakerId;
    bool interpolated = false;
};

using WordChunk = QVector<TimedWord>;

QString wordText(const QVariantMap &word)
{
    return word.value(QStringLiteral("text"), word.value(QStringLiteral("word")))
        .toString().trimmed();
}

bool sentenceEnd(const QString &text)
{
    static const QRegularExpression expression(
        QStringLiteral(R"([.!?\x{2026}\x{3002}\x{FF01}\x{FF1F}\x{0964}\x{061F}]["')\]]?$)"));
    return expression.match(text.trimmed()).hasMatch();
}

bool clauseEnd(const QString &text)
{
    static const QRegularExpression expression(
        QStringLiteral(R"([,;:\x{2014}\x{3001}\x{FF0C}\x{061B}\x{060C}]$)"));
    return expression.match(text.trimmed()).hasMatch();
}

QString joinWords(const WordChunk &words)
{
    QStringList parts;
    parts.reserve(words.size());
    for (const TimedWord &word : words) {
        if (!word.text.isEmpty()) parts.append(word.text);
    }
    QString text = parts.join(QLatin1Char(' '));
    text.replace(QRegularExpression(QStringLiteral("\\s+([,.;:!?%])")), QStringLiteral("\\1"));
    text.replace(QRegularExpression(QStringLiteral("([([{])\\s+")), QStringLiteral("\\1"));
    return text.simplified();
}

int chunkCharacters(const WordChunk &chunk)
{
    int count = 0;
    for (const TimedWord &word : chunk) count += word.text.size() + 1;
    return qMax(0, count - 1);
}

qint64 chunkDuration(const WordChunk &chunk)
{
    return chunk.isEmpty() ? 0 : qMax<qint64>(0, chunk.constLast().endMs - chunk.constFirst().startMs);
}

QVector<TimedWord> extractWords(const QVariantList &segments)
{
    QVector<TimedWord> result;
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
        const QString parentId = segment.value(QStringLiteral("id")).toString();
        const QString speakerId = segment.value(QStringLiteral("speakerId"),
                                                QStringLiteral("speaker-1")).toString();
        const QVariantList alignedWords = segment.value(QStringLiteral("words")).toList();
        if (!alignedWords.isEmpty()) {
            const QStringList sourceWords = segment.value(QStringLiteral("sourceText")).toString()
                .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            const bool preserveSourceSpelling = sourceWords.size() == alignedWords.size();
            for (int wordIndex = 0; wordIndex < alignedWords.size(); ++wordIndex) {
                const QVariant &wordEntry = alignedWords.at(wordIndex);
                const QVariantMap word = wordEntry.toMap();
                const QString text = preserveSourceSpelling ? sourceWords.at(wordIndex) : wordText(word);
                const qint64 start = word.value(QStringLiteral("startMs")).toLongLong();
                const qint64 end = word.value(QStringLiteral("endMs")).toLongLong();
                if (text.isEmpty() || end <= start) continue;
                result.append({text, start, end,
                               word.value(QStringLiteral("confidence")).toDouble(),
                               segment, parentId, speakerId, false});
            }
            continue;
        }

        const QStringList textWords = segment.value(QStringLiteral("sourceText")).toString()
            .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        const qint64 start = segment.value(QStringLiteral("startMs")).toLongLong();
        const qint64 end = segment.value(QStringLiteral("endMs")).toLongLong();
        const qint64 duration = end - start;
        if (textWords.isEmpty() || duration <= 0) continue;
        for (int i = 0; i < textWords.size(); ++i) {
            const qint64 wordStart = start + qRound64(double(i) / textWords.size() * duration);
            const qint64 wordEnd = start + qRound64(double(i + 1) / textWords.size() * duration);
            result.append({textWords.at(i), wordStart, qMax(wordStart + 1, wordEnd), 0.0,
                           segment, parentId, speakerId, true});
        }
    }
    return result;
}

QVector<WordChunk> buildSentenceChunks(const QVector<TimedWord> &words)
{
    QVector<WordChunk> chunks;
    WordChunk current;
    for (int i = 0; i < words.size(); ++i) {
        const TimedWord &word = words.at(i);
        if (!current.isEmpty() && word.speakerId != current.constLast().speakerId) {
            chunks.append(current);
            current.clear();
        }
        current.append(word);

        if (i + 1 >= words.size()) continue;
        const TimedWord &next = words.at(i + 1);
        const qint64 duration = chunkDuration(current);
        const qint64 nextGap = qMax<qint64>(0, next.startMs - word.endMs);
        const bool atIdeal = duration >= kIdealSegmentMs && chunkCharacters(current) >= kMinChars;
        const bool tooLong = duration >= kMaxSegmentMs || chunkCharacters(current) >= kMaxChars;
        const bool endsSentence = sentenceEnd(word.text);
        const bool endsClause = clauseEnd(word.text);
        const bool speakerChange = next.speakerId != word.speakerId;
        const bool naturalPause = nextGap >= kPauseBreakMs;

        const bool shouldFlush = speakerChange
            || (endsSentence && (atIdeal || naturalPause))
            || (tooLong && (endsSentence || endsClause || nextGap >= kForcePauseMs))
            || tooLong;
        if (shouldFlush) {
            chunks.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty()) chunks.append(current);
    return chunks;
}

bool isShort(const WordChunk &chunk)
{
    return chunkDuration(chunk) < kMinSegmentMs || chunkCharacters(chunk) < kMinChars
        || chunk.size() < kMinWords;
}

void mergeShortChunks(QVector<WordChunk> &chunks)
{
    for (int pass = 0; pass < 32; ++pass) {
        bool changed = false;
        for (int i = 0; i < chunks.size(); ++i) {
            if (!isShort(chunks.at(i))) continue;
            const WordChunk &chunk = chunks.at(i);
            const bool protectedSentence = sentenceEnd(chunk.constLast().text)
                && i + 1 < chunks.size()
                && chunks.at(i + 1).constFirst().startMs - chunk.constLast().endMs >= kPauseBreakMs;
            if (protectedSentence) continue;

            const bool canPrevious = i > 0
                && chunks.at(i - 1).constLast().speakerId == chunk.constFirst().speakerId
                && chunk.constFirst().startMs - chunks.at(i - 1).constLast().endMs <= kPauseBreakMs
                && chunk.constLast().endMs - chunks.at(i - 1).constFirst().startMs
                       <= kMaxSegmentMs + 1000
                && chunks.at(i - 1).size() + chunk.size() > 0;
            const bool canNext = i + 1 < chunks.size()
                && chunks.at(i + 1).constFirst().speakerId == chunk.constFirst().speakerId
                && chunks.at(i + 1).constFirst().startMs - chunk.constLast().endMs <= kPauseBreakMs
                && chunks.at(i + 1).constLast().endMs - chunk.constFirst().startMs
                       <= kMaxSegmentMs + 1000;
            if (!canPrevious && !canNext) continue;

            if (canPrevious && (!canNext || chunkDuration(chunks.at(i - 1))
                                           <= chunkDuration(chunks.at(i + 1)))) {
                chunks[i - 1] += chunk;
                chunks.removeAt(i);
            } else {
                WordChunk merged = chunk;
                merged += chunks.at(i + 1);
                chunks[i + 1] = merged;
                chunks.removeAt(i);
            }
            changed = true;
            break;
        }
        if (!changed) break;
    }
}

QVariantMap makeSegment(const WordChunk &chunk)
{
    QVariantMap segment = chunk.constFirst().source;
    segment.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    segment.insert(QStringLiteral("startMs"), chunk.constFirst().startMs);
    segment.insert(QStringLiteral("endMs"), chunk.constLast().endMs);
    segment.insert(QStringLiteral("sourceText"), joinWords(chunk));
    segment.insert(QStringLiteral("targetText"), QString());
    segment.insert(QStringLiteral("speakerId"), chunk.constFirst().speakerId);
    segment.insert(QStringLiteral("state"), QStringLiteral("transcribed"));

    QVariantList words;
    QStringList parentIds;
    bool interpolated = false;
    for (const TimedWord &word : chunk) {
        words.append(QVariantMap{{QStringLiteral("text"), word.text},
                                 {QStringLiteral("startMs"), word.startMs},
                                 {QStringLiteral("endMs"), word.endMs},
                                 {QStringLiteral("confidence"), word.confidence}});
        if (!word.parentId.isEmpty() && !parentIds.contains(word.parentId))
            parentIds.append(word.parentId);
        interpolated = interpolated || word.interpolated;
    }
    segment.insert(QStringLiteral("words"), words);
    if (parentIds.size() == 1)
        segment.insert(QStringLiteral("derivedFromSegmentId"), parentIds.constFirst());
    else if (parentIds.size() > 1) {
        QVariantList ids;
        for (const QString &id : parentIds) ids.append(id);
        segment.insert(QStringLiteral("derivedFromSegmentIds"), ids);
    }
    segment.insert(QStringLiteral("timingSource"), interpolated
                   ? QStringLiteral("asr-interpolated") : QStringLiteral("asr-word"));
    if (interpolated)
        segment.insert(QStringLiteral("alignmentDiagnostic"),
                       QStringLiteral("Some word timings were interpolated because aligned timestamps were unavailable."));
    return segment;
}

} // namespace

QVariantList DubbingSegmentNormalizer::normalize(const QVariantList &segments)
{
    const QVector<TimedWord> words = extractWords(segments);
    if (words.isEmpty()) return segments;

    QVector<WordChunk> chunks = buildSentenceChunks(words);
    mergeShortChunks(chunks);

    QVariantList result;
    result.reserve(chunks.size());
    for (const WordChunk &chunk : chunks) {
        if (!chunk.isEmpty()) result.append(makeSegment(chunk));
    }
    return result;
}

} // namespace LAStudio
