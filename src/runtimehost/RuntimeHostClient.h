#pragma once

#include "RuntimeHostProtocol.h"
#include "RuntimeHostSharedBuffer.h"

#include <QObject>
#include <QProcess>
#include <QLocalSocket>

#include <functional>
#include <atomic>

namespace LAStudio {

class RuntimeHostClient final : public QObject {
    Q_OBJECT
public:
    using ProgressCallback = std::function<void(const QCborMap &payload)>;

    explicit RuntimeHostClient(QObject *parent = nullptr);
    ~RuntimeHostClient() override;

    bool start(const QString &hostExecutable, QString *error = nullptr);
    bool isRunning() const;
    bool ping(QString *error = nullptr);
    bool shutdown(QString *error = nullptr);
    bool load(const QCborMap &configuration, QCborValue *schema, QString *error = nullptr);
    bool infer(const QCborMap &request,
               const QVector<float> &referenceSamples,
               QVector<float> *samples,
               int *sampleRate,
               QString *error = nullptr);
    bool execute(const QCborMap &request,
                 const QVector<float> &referenceSamples,
                 QCborMap *resultPayload,
                 QVector<float> *samples = nullptr,
                 int *sampleRate = nullptr,
                 QString *error = nullptr,
                 int inputSampleRate = 24000);
    bool request(RuntimeHostMessage message,
                 const QCborMap &payload,
                 RuntimeHostFrame *response,
                 QString *error = nullptr);
    void cancelCurrent();
    void setProgressCallback(ProgressCallback callback) { m_progressCallback = std::move(callback); }

private:
    bool connectSocket(QLocalSocket *socket, QString *error);
    bool authenticate(QLocalSocket *socket, QString *error);
    bool sendFrame(QLocalSocket *socket,
                   RuntimeHostMessage message,
                   quint64 requestId,
                   const QCborMap &payload,
                   QString *error);
    bool readResponse(QLocalSocket *socket,
                      quint64 requestId,
                      RuntimeHostFrame *response,
                      QString *error,
                      int timeoutMs = 10000);
    QString protocolError(const RuntimeHostFrame &frame) const;

    QProcess m_process;
    QLocalSocket m_socket;
    QString m_socketName;
    QString m_token;
    std::atomic<quint64> m_nextRequestId{1};
    std::atomic<quint64> m_currentRequestId{0};
    ProgressCallback m_progressCallback;
};

} // namespace LAStudio
