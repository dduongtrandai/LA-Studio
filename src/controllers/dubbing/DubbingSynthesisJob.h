#pragma once

#include <QObject>
#include <QByteArray>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace LAStudio {

class TtsEngine;

class DubbingSynthesisJob final : public QObject
{
    Q_OBJECT
public:
    explicit DubbingSynthesisJob(TtsEngine *tts, QObject *parent = nullptr);

    bool running() const { return m_running; }
    bool start(const QVariantList &segments, const QString &projectPath,
               const QVariantMap &settings, const QString &runId);
    void cancel();

signals:
    void progressChanged(int progress);
    void segmentUpdated(int index, const QVariantMap &segment);
    void completed(const QVariantList &segments);
    void failed(const QString &message);

private slots:
    void onSynthesisFinished(const QByteArray &pcm16, int sampleRate);
    void onTtsError(const QString &message);

private:
    void startCurrentChunk();
    void fitGeneratedSegments();
    void fail(const QString &message);

    TtsEngine *m_tts = nullptr;
    bool m_running = false;
    QVariantList m_segments;
    QString m_projectPath;
    QVariantMap m_settings;
    QString m_runId;
    QString m_nodeRunId;
    int m_generationIndex = -1;
    QVariantList m_chunks;
    int m_chunkIndex = -1;
    QVector<float> m_chunkSamples;
    int m_chunkSampleRate = 0;
};

} // namespace LAStudio
