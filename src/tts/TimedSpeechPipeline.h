#pragma once

#include "subtitles/TimedTextCue.h"
#include "SubtitleSmartFitPlanner.h"

#include <QObject>
#include <QVariantMap>
#include <QVector>

#include <memory>

class QTemporaryDir;

namespace LAStudio {

class TtsEngine;

class TimedSpeechPipeline final : public QObject
{
    Q_OBJECT

public:
    explicit TimedSpeechPipeline(TtsEngine *tts, QObject *parent = nullptr);

    bool processing() const { return m_processing; }
    bool start(const QVector<TimedTextCue> &cues, const QVariantMap &settings);
    void cancel();

signals:
    void cueUpdated(int index, const QVariantMap &patch);
    void phaseChanged(const QString &phase);
    void progressChanged(int progress);
    void errorOccurred(const QString &message);
    void finished(const QString &outputPath, const QVariantMap &summary);

private slots:
    void onTtsFinished(const QByteArray &pcm16, int sampleRate);
    void onTtsError(const QString &message);

private:
    void setPhase(const QString &phase);
    void startCue(int index);
    void fitAndAssemble();
    void updateCue(int index, const QVariantMap &patch);
    void resetJobDirectory();

    TtsEngine *m_tts = nullptr;
    bool m_processing = false;
    bool m_cancelRequested = false;
    bool m_awaitingSynthesis = false;
    int m_currentCue = -1;
    int m_sampleRate = 0;
    QString m_phase = QStringLiteral("idle");
    QString m_ttsSignature;
    QVariantMap m_ttsSettings;
    QVector<TimedTextCue> m_cues;
    QVector<QString> m_naturalPaths;
    QVector<qint64> m_naturalDurationsMs;
    QVector<SubtitleFit> m_initialFits;
    std::unique_ptr<QTemporaryDir> m_jobDirectory;
};

} // namespace LAStudio
