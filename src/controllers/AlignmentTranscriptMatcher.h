#pragma once

#include <QString>
#include <QVariantList>
#include <QVector>

namespace LAStudio {

struct AlignmentMatchResult {
    QVector<QVector<int>> canonicalTokensByChunk;
    double score = 0.0;
    double coverage = 0.0;
};

class AlignmentTranscriptMatcher {
public:
    struct Token {
        QString normalized;
        int line = -1;
    };

    static QString normalize(const QString &text);
    static QVector<Token> tokenizeCanonical(const QStringList &lines, const QString &language);
    static QVector<QString> tokenizeText(const QString &text, const QString &language);
    static AlignmentMatchResult match(const QStringList &canonicalLines,
                                      const QStringList &chunkTexts,
                                      const QString &language);
};

} // namespace LAStudio
