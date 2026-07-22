#pragma once

#include "RuntimeHostProtocol.h"
#include "RuntimeHostSharedBuffer.h"
#include "RuntimeHostAdapter.h"

#include <QObject>
#include <QHash>
#include <QLocalServer>
#include <QThread>
#include <QVector>

#include <memory>

class QLocalSocket;
class QCborValue;

namespace LAStudio {

class RuntimeHostWorker;

// Minimal host shell. Runtime adapters are added behind this server so the
// process/IPC contract can be tested independently from any vendor DLL.
class RuntimeHostServer final : public QObject {
    Q_OBJECT
public:
    explicit RuntimeHostServer(QString socketName, QString token, QObject *parent = nullptr);
    ~RuntimeHostServer() override;
    bool start(QString *error = nullptr);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onWorkerLoadFinished(bool ok, QCborValue schema, QString error);
    void onWorkerInferFinished(bool ok, RuntimeHostAdapter::Result result, QString error);
    void onWorkerProgress(int current, int total, QString stage, int chunkIndex, int chunkCount);
    void onWorkerCancelled();

private:
    struct ClientState {
        RuntimeHostFrameParser parser;
        bool authenticated = false;
    };

    void handleFrame(QLocalSocket *socket, const RuntimeHostFrame &frame);
    void send(QLocalSocket *socket,
              RuntimeHostMessage message,
              quint64 requestId,
              const QCborMap &payload = {});
    void reject(QLocalSocket *socket, quint64 requestId, const QString &message);

    QString m_socketName;
    QString m_token;
    QLocalServer m_server;
    QHash<QLocalSocket *, ClientState> m_clients;
    QThread m_workerThread;
    RuntimeHostWorker *m_worker = nullptr;
    RuntimeHostSharedBuffer m_outputBuffer;
    QString m_adapterId;
    QLocalSocket *m_pendingSocket = nullptr;
    quint64 m_pendingRequestId = 0;
    bool m_inferencePending = false;
};

} // namespace LAStudio
