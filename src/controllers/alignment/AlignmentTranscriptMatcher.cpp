#include "controllers/alignment/AlignmentTranscriptMatcher.h"

#include <QChar>
#include <QtMath>
#include <algorithm>

namespace LAStudio {
namespace {

bool isCjkLanguage(const QString &language)
{
    const QString lang = language.left(3).toLower();
    return lang == QStringLiteral("cmn") || lang == QStringLiteral("zho") ||
           lang == QStringLiteral("zh") || lang == QStringLiteral("jpn") ||
           lang == QStringLiteral("ja") || lang == QStringLiteral("kor") ||
           lang == QStringLiteral("ko");
}

double tokenSimilarity(const QString &a, const QString &b)
{
    if (a == b) return 1.0;
    if (a.isEmpty() || b.isEmpty()) return 0.0;
    const int n = a.size(), m = b.size();
    QVector<int> prev(m + 1), curr(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        curr[0] = i;
        for (int j = 1; j <= m; ++j) {
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1,
                                prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        }
        prev.swap(curr);
    }
    return 1.0 - double(prev[m]) / double(std::max(n, m));
}

} // namespace

QString AlignmentTranscriptMatcher::normalize(const QString &text)
{
    const QString input = text.normalized(QString::NormalizationForm_KC).toLower();
    QString out;
    out.reserve(input.size());
    bool pendingSpace = false;
    for (const QChar ch : input) {
        if (ch.isLetterOrNumber()) {
            if (pendingSpace && !out.isEmpty()) out.append(QLatin1Char(' '));
            out.append(ch);
            pendingSpace = false;
        } else if (ch.isSpace()) {
            pendingSpace = true;
        }
    }
    return out.trimmed();
}

QVector<QString> AlignmentTranscriptMatcher::tokenizeText(const QString &text, const QString &language)
{
    const QString normalized = normalize(text);
    QVector<QString> result;
    if (isCjkLanguage(language)) {
        for (const QChar ch : normalized) if (!ch.isSpace()) result.append(QString(ch));
    } else {
        const QStringList words = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        result.reserve(words.size());
        for (const QString &word : words) result.append(word);
    }
    return result;
}

QVector<AlignmentTranscriptMatcher::Token> AlignmentTranscriptMatcher::tokenizeCanonical(
    const QStringList &lines, const QString &language)
{
    QVector<Token> result;
    for (int line = 0; line < lines.size(); ++line) {
        const auto tokens = tokenizeText(lines[line], language);
        for (const QString &token : tokens) result.append({token, line});
    }
    return result;
}

AlignmentMatchResult AlignmentTranscriptMatcher::match(const QStringList &canonicalLines,
                                                        const QStringList &chunkTexts,
                                                        const QString &language)
{
    const auto canonical = tokenizeCanonical(canonicalLines, language);
    QVector<QString> observed;
    QVector<int> observedChunks;
    for (int chunk = 0; chunk < chunkTexts.size(); ++chunk) {
        const auto tokens = tokenizeText(chunkTexts[chunk], language);
        for (const QString &token : tokens) {
            observed.append(token);
            observedChunks.append(chunk);
        }
    }

    AlignmentMatchResult result;
    result.canonicalTokensByChunk.resize(chunkTexts.size());
    const int n = canonical.size(), m = observed.size();
    if (n == 0 || m == 0) return result;

    // Global monotonic Needleman-Wunsch. Scores reward exact/fuzzy matches and
    // penalize missing/extra tokens, preventing independent chunks from jumping
    // to repeated text later in the transcript.
    const double gap = -0.9;
    QVector<QVector<double>> dp(n + 1, QVector<double>(m + 1));
    QVector<QVector<quint8>> trace(n + 1, QVector<quint8>(m + 1));
    for (int i = 1; i <= n; ++i) { dp[i][0] = i * gap; trace[i][0] = 1; }
    for (int j = 1; j <= m; ++j) { dp[0][j] = j * gap; trace[0][j] = 2; }
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            const double sim = tokenSimilarity(canonical[i - 1].normalized, observed[j - 1]);
            const double diagonal = dp[i - 1][j - 1] + (sim >= 0.7 ? 2.0 * sim : -1.0);
            const double up = dp[i - 1][j] + gap;
            const double left = dp[i][j - 1] + gap;
            if (diagonal >= up && diagonal >= left) { dp[i][j] = diagonal; trace[i][j] = 0; }
            else if (up >= left) { dp[i][j] = up; trace[i][j] = 1; }
            else { dp[i][j] = left; trace[i][j] = 2; }
        }
    }

    int i = n, j = m, matched = 0;
    double similaritySum = 0.0;
    while (i > 0 || j > 0) {
        const quint8 step = trace[i][j];
        if (i > 0 && j > 0 && step == 0) {
            const double sim = tokenSimilarity(canonical[i - 1].normalized, observed[j - 1]);
            if (sim >= 0.7) {
                result.canonicalTokensByChunk[observedChunks[j - 1]].prepend(i - 1);
                ++matched;
                similaritySum += sim;
            }
            --i; --j;
        } else if (i > 0 && (j == 0 || step == 1)) --i;
        else if (j > 0) --j;
    }
    result.coverage = double(matched) / double(n);
    result.score = matched > 0 ? similaritySum / double(matched) * result.coverage : 0.0;
    return result;
}

} // namespace LAStudio
