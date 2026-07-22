#pragma once

#include <QVariantList>
#include <QVariantMap>
#include <QString>
#include <QStringList>

namespace LAStudio {

struct DubbingDurationSettings
{
    bool enabled = true;
    QString unit = QStringLiteral("phoneme-v1");
    double safetyFactor = 1.0;
    double lowerToleranceRatio = 0.20;
    double upperToleranceRatio = 0.20;
    int pauseThresholdMs = 250;
    int maxPreTtsIterations = 4;
    int candidatesPerIteration = 3;
    int maxPostTtsRepairs = 0;
    double maxFitRate = 1.12;

    QVariantMap toVariantMap() const;
    static DubbingDurationSettings fromVariantMap(const QVariantMap &map);
};

struct DubbingPause
{
    qint64 offsetMs = 0;
    qint64 durationMs = 0;
    QString kind;

    QVariantMap toVariantMap() const;
};

struct DubbingSpeechBudget
{
    qint64 slotMs = 0;
    qint64 speechWindowMs = 0;
    double phonemesPerSecond = 0.0;
    int targetUnits = 0;
    int minUnits = 0;
    int maxUnits = 0;
    double confidence = 0.0;
    QVariantList pauses;

    QVariantMap toVariantMap() const;
};

class DubbingDurationPlanner final
{
public:
    static QVariantList extractPauses(const QVariantMap &segment,
                                      int thresholdMs = 250);
    static DubbingSpeechBudget plan(const QVariantMap &segment,
                                    double phonemesPerSecond,
                                    const DubbingDurationSettings &settings = {});
    static int countPhonemes(const QString &text,
                             const QString &language = QStringLiteral("vi"));
    static int countVietnameseSyllables(const QString &text);
    static int phonemeDistance(const QString &text, int predictedPhonemes,
                               const QString &language = QStringLiteral("vi"));
    static double semanticFidelityScore(const QString &reference,
                                        const QString &candidate);
    static QString selectBestCandidate(const QString &sourceText,
                                       const QString &reference,
                                       const QString &current,
                                       const QStringList &candidates,
                                       int predictedPhonemes,
                                       int minPhonemes,
                                       int maxPhonemes,
                                       const QStringList &protectedTokens,
                                       const QString &language = QStringLiteral("vi"));
    static QVariantList pauseChunks(const QString &markedText,
                                    const QVariantList &sourcePauses);
    static QString textWithoutPauseMarkers(const QString &markedText);
    static QString normalizeSpokenVietnamese(const QString &text);
};

} // namespace LAStudio
