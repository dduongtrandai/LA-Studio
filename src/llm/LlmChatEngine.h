#pragma once

#include <QObject>
#include <QList>
#include <QVariantMap>
#include <QThread>
#include <atomic>
#include <memory>
#include <QtQml/qqml.h>
#include <QtQml/qqmlregistration.h>

namespace LAStudio {

class LlmChatEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LlmChatEngine is managed by AppController")
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
public:
    enum State { Unloaded, Loading, Ready, Processing, Error };
    Q_ENUM(State)

    explicit LlmChatEngine(QObject *parent = nullptr);
    ~LlmChatEngine() override;

    State state() const { return m_state; }
    bool isModelLoaded() const { return m_modelLoaded; }
    bool isProcessing() const { return m_processing; }

    void load(const QString &runtimePath, const QString &modelPath, bool useGpu);
    void unload();
    void generate(const QList<QVariantMap> &messages, int contextTokens, int maxTokens,
                  float temperature, float topP, int topK, float repeatPenalty,
                  const QString &requestId);
    void cancel();

signals:
    void stateChanged();
    void modelLoadedChanged();
    void processingChanged();
    void tokenGenerated(const QString &requestId, const QString &token);
    void generationFinished(const QString &requestId, const QString &text);
    void generationCancelled(const QString &requestId, const QString &text);
    void errorOccurred(const QString &message);

private slots:
    void onLoaded(bool ok, const QString &error);
    void onUnloaded();
    void onToken(const QString &requestId, const QString &token);
    void onFinished(const QString &requestId, const QString &text);
    void onCancelled(const QString &requestId, const QString &text);
    void onError(const QString &requestId, const QString &message);

private:
    class Worker;
    Worker *m_worker = nullptr;
    QThread m_thread;
    State m_state = Unloaded;
    bool m_modelLoaded = false;
    bool m_processing = false;
    QString m_requestId;
};

} // namespace LAStudio
