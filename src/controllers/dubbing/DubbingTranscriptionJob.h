#pragma once

#include <QObject>
#include <QFutureWatcher>
#include <QAtomicInteger>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

namespace LAStudio {

class SttSessionController;
class ModelManager;
class RuntimeManager;

class DubbingTranscriptionJob final : public QObject
{
    Q_OBJECT
public:
    DubbingTranscriptionJob(SttSessionController *stt, ModelManager *models,
                            RuntimeManager *runtimes, QObject *parent = nullptr);
    ~DubbingTranscriptionJob() override;

    bool running() const { return m_running; }
    bool start(const QString &language, const QString &audioPath,
               const QString &fallbackAudioPath = QString());
    void cancel();

signals:
    void progressChanged(int progress);
    void completed(const QVariantList &segments);
    void failed(const QString &message);

private slots:
    void onTranscriptionFinished(const QString &text, const QVariantList &segments);
    void onAlignmentFinished();

private:
    void beginAlignment(const QVariantList &segments);
    void startAudioInput(const QString &audioPath);
    void fail(const QString &message);

    SttSessionController *m_stt = nullptr;
    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;
    QFutureWatcher<QVariantMap> *m_alignmentWatcher = nullptr;
    std::shared_ptr<QAtomicInteger<bool>> m_alignmentCancel;
    QString m_audioPath;
    QString m_fallbackAudioPath;
    QString m_language;
    bool m_waitingForInput = false;
    bool m_running = false;
    bool m_retriedWithFallback = false;
    quint64 m_generation = 0;
};

} // namespace LAStudio
