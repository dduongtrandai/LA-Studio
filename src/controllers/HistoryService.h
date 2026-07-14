#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QThread>
#include <QtQml/qqml.h>

namespace LAStudio {

class TtsEngine;
class AudioRecorder;
class HistoryWorker;

class HistoryService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("HistoryService is managed by AppController")

    Q_PROPERTY(QVariantList ttsHistory READ ttsHistory NOTIFY ttsHistoryChanged)
    Q_PROPERTY(QVariantList sttHistory READ sttHistory NOTIFY sttHistoryChanged)
    Q_PROPERTY(QVariantList voiceDesignHistory READ voiceDesignHistory NOTIFY voiceDesignHistoryChanged)

public:
    explicit HistoryService(TtsEngine* tts, AudioRecorder* recorder, QObject *parent = nullptr);
    ~HistoryService() override;

    QVariantList ttsHistory() const { return m_ttsHistory; }
    QVariantList sttHistory() const { return m_sttHistory; }
    QVariantList voiceDesignHistory() const { return m_voiceDesignHistory; }

    Q_INVOKABLE void addTtsHistoryItem(const QString &text, const QString &modelName, const QString &voiceName);
    Q_INVOKABLE void deleteTtsHistoryItem(const QString &id);
    Q_INVOKABLE void clearTtsHistory();

    void addSttHistoryItem(const QString &text, const QString &modelName, const QVector<float> &samples);
    Q_INVOKABLE void deleteSttHistoryItem(const QString &id);
    Q_INVOKABLE void clearSttHistory();

    Q_INVOKABLE void addVoiceDesignHistoryItem(const QString &text, const QString &voiceDescription, const QString &presetName, const QString &familyId, const QString &modelName);
    Q_INVOKABLE void deleteVoiceDesignHistoryItem(const QString &id);
    Q_INVOKABLE void clearVoiceDesignHistory();

    void loadTtsHistory();
    void loadSttHistory();
    void loadVoiceDesignHistory();

signals:
    void ttsHistoryChanged();
    void sttHistoryChanged();
    void voiceDesignHistoryChanged();
    void errorOccurred(const QString &msg);

private slots:
    void onTtsHistoryLoaded(const QVariantList &list);
    void onSttHistoryLoaded(const QVariantList &list);
    void onVoiceDesignHistoryLoaded(const QVariantList &list);

private:
    TtsEngine* m_tts = nullptr;
    AudioRecorder* m_recorder = nullptr;

    QVariantList m_ttsHistory;
    QVariantList m_sttHistory;
    QVariantList m_voiceDesignHistory;

    QThread m_workerThread;
    HistoryWorker *m_worker = nullptr;
};

} // namespace LAStudio
