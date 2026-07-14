#include "HistoryService.h"
#include "HistoryRepository.h"

#include "core/PathUtils.h"
#include "tts/TtsEngine.h"
#include "audio/AudioRecorder.h"
#include "core/Logger.h"

#include <QMetaObject>
#include <QPointer>

namespace LAStudio {

class HistoryWorker : public QObject {
    Q_OBJECT
public:
    explicit HistoryWorker(const QString &dataDir, QObject *parent = nullptr)
        : QObject(parent)
        , m_dataDir(dataDir)
    {}

public slots:
    void loadTtsHistory() {
        QVariantList list = HistoryRepository::loadTtsHistory(m_dataDir);
        emit ttsHistoryLoaded(list);
    }

    void addTtsHistoryItem(const QString &text, const QString &modelName, const QString &voiceName, const QVector<float> &samples, int sampleRate) {
        QString errorMsg;
        bool ok = HistoryRepository::addTtsHistoryItem(m_dataDir, text, modelName, voiceName, samples, sampleRate, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadTtsHistory();
        }
    }

    void deleteTtsHistoryItem(const QString &id) {
        QString errorMsg;
        bool ok = HistoryRepository::deleteTtsHistoryItem(m_dataDir, id, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadTtsHistory();
        }
    }

    void clearTtsHistory() {
        QString errorMsg;
        bool ok = HistoryRepository::clearTtsHistory(m_dataDir, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadTtsHistory();
        }
    }

    void loadSttHistory() {
        QVariantList list = HistoryRepository::loadSttHistory(m_dataDir);
        emit sttHistoryLoaded(list);
    }

    void addSttHistoryItem(const QString &text, const QString &modelName, const QVector<float> &samples) {
        QString errorMsg;
        bool ok = HistoryRepository::addSttHistoryItem(m_dataDir, text, modelName, samples, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadSttHistory();
        }
    }

    void deleteSttHistoryItem(const QString &id) {
        QString errorMsg;
        bool ok = HistoryRepository::deleteSttHistoryItem(m_dataDir, id, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadSttHistory();
        }
    }

    void clearSttHistory() {
        QString errorMsg;
        bool ok = HistoryRepository::clearSttHistory(m_dataDir, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadSttHistory();
        }
    }

    void loadVoiceDesignHistory() {
        QVariantList list = HistoryRepository::loadVoiceDesignHistory(m_dataDir);
        emit voiceDesignHistoryLoaded(list);
    }

    void addVoiceDesignHistoryItem(const QString &text, const QString &voiceDescription, const QString &presetName, const QString &familyId, const QString &modelName, const QVector<float> &samples, int sampleRate) {
        QString errorMsg;
        bool ok = HistoryRepository::addVoiceDesignHistoryItem(m_dataDir, text, voiceDescription, presetName, familyId, modelName, samples, sampleRate, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadVoiceDesignHistory();
        }
    }

    void deleteVoiceDesignHistoryItem(const QString &id) {
        QString errorMsg;
        bool ok = HistoryRepository::deleteVoiceDesignHistoryItem(m_dataDir, id, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadVoiceDesignHistory();
        }
    }

    void clearVoiceDesignHistory() {
        QString errorMsg;
        bool ok = HistoryRepository::clearVoiceDesignHistory(m_dataDir, errorMsg);
        if (!ok) {
            emit errorOccurred(errorMsg);
        } else {
            loadVoiceDesignHistory();
        }
    }

signals:
    void ttsHistoryLoaded(const QVariantList &list);
    void sttHistoryLoaded(const QVariantList &list);
    void voiceDesignHistoryLoaded(const QVariantList &list);
    void errorOccurred(const QString &msg);

private:
    QString m_dataDir;
};

HistoryService::HistoryService(TtsEngine* tts, AudioRecorder* recorder, QObject *parent)
    : QObject(parent)
    , m_tts(tts)
    , m_recorder(recorder)
{
    m_worker = new HistoryWorker(PathUtils::dataDir());
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &HistoryWorker::ttsHistoryLoaded, this, &HistoryService::onTtsHistoryLoaded);
    connect(m_worker, &HistoryWorker::sttHistoryLoaded, this, &HistoryService::onSttHistoryLoaded);
    connect(m_worker, &HistoryWorker::voiceDesignHistoryLoaded, this, &HistoryService::onVoiceDesignHistoryLoaded);
    connect(m_worker, &HistoryWorker::errorOccurred, this, &HistoryService::errorOccurred);

    m_workerThread.start();

    loadTtsHistory();
    loadSttHistory();
    loadVoiceDesignHistory();
}

HistoryService::~HistoryService()
{
    m_workerThread.quit();
    m_workerThread.wait();
}

void HistoryService::loadTtsHistory()
{
    QMetaObject::invokeMethod(m_worker, "loadTtsHistory", Qt::QueuedConnection);
}

void HistoryService::loadSttHistory()
{
    QMetaObject::invokeMethod(m_worker, "loadSttHistory", Qt::QueuedConnection);
}

void HistoryService::addTtsHistoryItem(const QString &text, const QString &modelName, const QString &voiceName)
{
    if (!m_tts || m_tts->lastSamples().isEmpty()) {
        return;
    }

    QVector<float> samples = m_tts->lastSamples();
    int sampleRate = m_tts->sampleRate();

    QMetaObject::invokeMethod(m_worker, [this, text, modelName, voiceName, samples, sampleRate]() {
        m_worker->addTtsHistoryItem(text, modelName, voiceName, samples, sampleRate);
    });
}

void HistoryService::deleteTtsHistoryItem(const QString &id)
{
    QMetaObject::invokeMethod(m_worker, [this, id]() {
        m_worker->deleteTtsHistoryItem(id);
    });
}

void HistoryService::clearTtsHistory()
{
    QMetaObject::invokeMethod(m_worker, [this]() {
        m_worker->clearTtsHistory();
    });
}

void HistoryService::addSttHistoryItem(const QString &text, const QString &modelName, const QVector<float> &samples)
{
    QMetaObject::invokeMethod(m_worker, [this, text, modelName, samples]() {
        m_worker->addSttHistoryItem(text, modelName, samples);
    });
}

void HistoryService::deleteSttHistoryItem(const QString &id)
{
    QMetaObject::invokeMethod(m_worker, [this, id]() {
        m_worker->deleteSttHistoryItem(id);
    });
}

void HistoryService::clearSttHistory()
{
    QMetaObject::invokeMethod(m_worker, [this]() {
        m_worker->clearSttHistory();
    });
}

void HistoryService::loadVoiceDesignHistory()
{
    QMetaObject::invokeMethod(m_worker, "loadVoiceDesignHistory", Qt::QueuedConnection);
}

void HistoryService::addVoiceDesignHistoryItem(const QString &text, const QString &voiceDescription, const QString &presetName, const QString &familyId, const QString &modelName)
{
    if (!m_tts || m_tts->lastSamples().isEmpty()) {
        return;
    }

    QVector<float> samples = m_tts->lastSamples();
    int sampleRate = m_tts->sampleRate();

    QMetaObject::invokeMethod(m_worker, [this, text, voiceDescription, presetName, familyId, modelName, samples, sampleRate]() {
        m_worker->addVoiceDesignHistoryItem(text, voiceDescription, presetName, familyId, modelName, samples, sampleRate);
    });
}

void HistoryService::deleteVoiceDesignHistoryItem(const QString &id)
{
    QMetaObject::invokeMethod(m_worker, [this, id]() {
        m_worker->deleteVoiceDesignHistoryItem(id);
    });
}

void HistoryService::clearVoiceDesignHistory()
{
    QMetaObject::invokeMethod(m_worker, [this]() {
        m_worker->clearVoiceDesignHistory();
    });
}

void HistoryService::onTtsHistoryLoaded(const QVariantList &list)
{
    m_ttsHistory = list;
    emit ttsHistoryChanged();
}

void HistoryService::onSttHistoryLoaded(const QVariantList &list)
{
    m_sttHistory = list;
    emit sttHistoryChanged();
}

void HistoryService::onVoiceDesignHistoryLoaded(const QVariantList &list)
{
    m_voiceDesignHistory = list;
    emit voiceDesignHistoryChanged();
}

} // namespace LAStudio

#include "HistoryService.moc"
