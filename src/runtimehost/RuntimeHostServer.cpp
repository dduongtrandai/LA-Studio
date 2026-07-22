#include "RuntimeHostServer.h"
#include "RuntimeHostWorker.h"

#include <QCoreApplication>
#include <QCborArray>
#include <QLocalSocket>

#include <utility>
#include <QMetaObject>

namespace LAStudio {

RuntimeHostServer::RuntimeHostServer(QString socketName, QString token, QObject *parent)
    : QObject(parent)
    , m_socketName(std::move(socketName))
    , m_token(std::move(token))
{
    qRegisterMetaType<RuntimeHostAdapter::Result>("LAStudio::RuntimeHostAdapter::Result");
    connect(&m_server, &QLocalServer::newConnection,
            this, &RuntimeHostServer::onNewConnection);
}

RuntimeHostServer::~RuntimeHostServer()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "unload", Qt::BlockingQueuedConnection);
    }
    m_workerThread.quit();
    m_workerThread.wait();
    delete m_worker;
    m_worker = nullptr;
}

bool RuntimeHostServer::start(QString *error)
{
    if (m_socketName.isEmpty() || m_token.isEmpty()) {
        if (error) *error = QStringLiteral("RuntimeHost socket name and token are required.");
        return false;
    }
    QLocalServer::removeServer(m_socketName);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(m_socketName)) {
        if (error) *error = m_server.errorString();
        return false;
    }
    return true;
}

void RuntimeHostServer::onNewConnection()
{
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        m_clients.insert(socket, ClientState{});
        connect(socket, &QLocalSocket::readyRead, this, &RuntimeHostServer::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &RuntimeHostServer::onDisconnected);
    }
}

void RuntimeHostServer::onReadyRead()
{
    auto *socket = qobject_cast<QLocalSocket *>(sender());
    if (!socket || !m_clients.contains(socket)) return;

    ClientState &state = m_clients[socket];
    state.parser.append(socket->readAll());
    while (true) {
        QString parseError;
        const auto frame = state.parser.takeNext(&parseError);
        if (!parseError.isEmpty()) {
            reject(socket, 0, parseError);
            socket->disconnectFromServer();
            return;
        }
        if (!frame.has_value()) return;
        handleFrame(socket, *frame);
        if (!m_clients.contains(socket)) return;
    }
}

void RuntimeHostServer::onDisconnected()
{
    auto *socket = qobject_cast<QLocalSocket *>(sender());
    if (!socket) return;
    if (socket == m_pendingSocket && m_worker) {
        m_worker->requestCancel();
    }
    m_clients.remove(socket);
    socket->deleteLater();
    if (m_clients.isEmpty()) QCoreApplication::quit();
}

void RuntimeHostServer::handleFrame(QLocalSocket *socket, const RuntimeHostFrame &frame)
{
    ClientState &state = m_clients[socket];
    QCborMap payload;
    QString decodeError;
    if (!decodeRuntimeHostCbor(frame.payload, &payload, &decodeError)) {
        reject(socket, frame.requestId, decodeError);
        return;
    }

    if (frame.message == RuntimeHostMessage::Hello) {
        const QString token = payload.value(QStringLiteral("token")).toString();
        if (token != m_token) {
            reject(socket, frame.requestId, QStringLiteral("RuntimeHost authentication failed."));
            socket->disconnectFromServer();
            return;
        }
        state.authenticated = true;
        send(socket, RuntimeHostMessage::HelloAck, frame.requestId,
             QCborMap{{QStringLiteral("protocolMajor"), kRuntimeHostProtocolMajor},
                      {QStringLiteral("protocolMinor"), kRuntimeHostProtocolMinor},
                      {QStringLiteral("capabilities"), QCborArray{
                          QStringLiteral("ping"), QStringLiteral("shutdown"),
                          QStringLiteral("audio-shmem"), QStringLiteral("structured-result"),
                          QStringLiteral("omnivoice"), QStringLiteral("whisper"),
                          QStringLiteral("llama")}}});
        return;
    }

    if (!state.authenticated) {
        reject(socket, frame.requestId, QStringLiteral("RuntimeHost handshake is required."));
        socket->disconnectFromServer();
        return;
    }

    switch (frame.message) {
    case RuntimeHostMessage::Load: {
        const QString adapterId = payload.value(QStringLiteral("adapter")).toString();
        // A host process is intentionally bound to one runtime family for its
        // entire lifetime.  Reusing it for a different adapter could leave a
        // previously loaded DLL (for example ggml.dll) in the process module
        // table even after the model object is destroyed.
        if (!m_adapterId.isEmpty() && m_adapterId != adapterId) {
            reject(socket, frame.requestId,
                   QStringLiteral("RuntimeHost is bound to adapter '%1'; start a new host for '%2'.")
                       .arg(m_adapterId, adapterId));
            break;
        }
        if (m_workerThread.isRunning()) {
            QMetaObject::invokeMethod(m_worker, "unload", Qt::BlockingQueuedConnection);
            m_workerThread.quit();
            m_workerThread.wait();
            delete m_worker;
            m_worker = nullptr;
        }
        m_worker = new RuntimeHostWorker;
        m_worker->moveToThread(&m_workerThread);
        connect(m_worker, &RuntimeHostWorker::loadFinished, this, &RuntimeHostServer::onWorkerLoadFinished);
        connect(m_worker, &RuntimeHostWorker::inferFinished, this, &RuntimeHostServer::onWorkerInferFinished);
        connect(m_worker, &RuntimeHostWorker::progress, this, &RuntimeHostServer::onWorkerProgress);
        connect(m_worker, &RuntimeHostWorker::cancelled, this, &RuntimeHostServer::onWorkerCancelled);
        m_pendingSocket = socket;
        m_pendingRequestId = frame.requestId;
        m_workerThread.start();
        m_adapterId = adapterId;
        QMetaObject::invokeMethod(m_worker, "load", Qt::QueuedConnection,
                                  Q_ARG(QString, adapterId), Q_ARG(QCborMap, payload));
        break;
    }
    case RuntimeHostMessage::Infer: {
        if (!m_worker || !m_workerThread.isRunning()) {
            reject(socket, frame.requestId, QStringLiteral("RuntimeHost model is not loaded."));
            break;
        }
        if (m_inferencePending) {
            reject(socket, frame.requestId, QStringLiteral("RuntimeHost is busy."));
            break;
        }
        QVector<float> referenceSamples;
        const QCborValue reference = payload.value(QStringLiteral("referenceBuffer"));
        if (reference.isMap()) {
            RuntimeHostSharedBuffer input;
            QString inputError;
            if (!input.attach(reference.toMap(), &inputError)
                || !input.copyTo(&referenceSamples, &inputError)) {
                reject(socket, frame.requestId, inputError);
                break;
            }
        }
        m_pendingSocket = socket;
        m_pendingRequestId = frame.requestId;
        m_inferencePending = true;
        QMetaObject::invokeMethod(m_worker, "infer", Qt::QueuedConnection,
                                  Q_ARG(QCborMap, payload), Q_ARG(QVector<float>, referenceSamples));
        break;
    }
    case RuntimeHostMessage::Cancel:
        if (m_worker) m_worker->requestCancel();
        send(socket, RuntimeHostMessage::Cancelled, frame.requestId, QCborMap{{QStringLiteral("ok"), true}});
        break;
    case RuntimeHostMessage::Unload:
        if (m_worker) QMetaObject::invokeMethod(m_worker, "unload", Qt::BlockingQueuedConnection);
        m_workerThread.quit();
        m_workerThread.wait();
        delete m_worker;
        m_worker = nullptr;
        m_adapterId.clear();
        m_outputBuffer.detach();
        send(socket, RuntimeHostMessage::UnloadResult, frame.requestId,
             QCborMap{{QStringLiteral("ok"), true}});
        break;
    case RuntimeHostMessage::Ping:
        send(socket, RuntimeHostMessage::Pong, frame.requestId,
             QCborMap{{QStringLiteral("ok"), true}});
        break;
    case RuntimeHostMessage::Shutdown:
        if (m_worker) QMetaObject::invokeMethod(m_worker, "unload", Qt::BlockingQueuedConnection);
        m_workerThread.quit();
        m_workerThread.wait();
        delete m_worker;
        m_worker = nullptr;
        m_outputBuffer.detach();
        send(socket, RuntimeHostMessage::UnloadResult, frame.requestId,
             QCborMap{{QStringLiteral("ok"), true}});
        QCoreApplication::quit();
        break;
    default:
        reject(socket, frame.requestId, QStringLiteral("RuntimeHost adapter is not installed."));
        break;
    }
}

void RuntimeHostServer::onWorkerLoadFinished(bool ok, QCborValue schema, QString error)
{
    if (!m_pendingSocket || !m_clients.contains(m_pendingSocket)) return;
    if (!ok) reject(m_pendingSocket, m_pendingRequestId, error);
    else send(m_pendingSocket, RuntimeHostMessage::LoadResult, m_pendingRequestId,
              QCborMap{{QStringLiteral("ok"), true}, {QStringLiteral("schema"), schema}});
    m_pendingSocket = nullptr;
}

void RuntimeHostServer::onWorkerInferFinished(bool ok, RuntimeHostAdapter::Result result, QString error)
{
    QLocalSocket *socket = m_pendingSocket;
    const quint64 requestId = m_pendingRequestId;
    m_pendingSocket = nullptr;
    m_inferencePending = false;
    if (!socket || !m_clients.contains(socket)) return;
    if (!ok) { reject(socket, requestId, error); return; }
    QCborMap response = result.payload;
    if (!result.samples.isEmpty()) {
        QCborMap descriptor;
        QString outputError;
        m_outputBuffer.detach();
        if (!m_outputBuffer.createFromSamples(result.samples, result.sampleRate, 1,
                                              &descriptor, &outputError)) {
            reject(socket, requestId, outputError); return;
        }
        response.insert(QStringLiteral("sampleRate"), result.sampleRate);
        response.insert(QStringLiteral("outputBuffer"), descriptor);
    }
    send(socket, RuntimeHostMessage::Completed, requestId, response);
}

void RuntimeHostServer::onWorkerProgress(int current, int total, QString stage,
                                          int chunkIndex, int chunkCount)
{
    if (!m_pendingSocket || !m_clients.contains(m_pendingSocket)) return;
    send(m_pendingSocket, RuntimeHostMessage::Progress, 0,
         QCborMap{{QStringLiteral("current"), current}, {QStringLiteral("total"), total},
                  {QStringLiteral("stage"), stage}, {QStringLiteral("chunkIndex"), chunkIndex},
                  {QStringLiteral("chunkCount"), chunkCount}});
}

void RuntimeHostServer::onWorkerCancelled() {}

void RuntimeHostServer::send(QLocalSocket *socket,
                             RuntimeHostMessage message,
                             quint64 requestId,
                             const QCborMap &payload)
{
    if (!socket) return;
    socket->write(encodeRuntimeHostFrame(message, requestId, encodeRuntimeHostCbor(payload)));
    socket->flush();
}

void RuntimeHostServer::reject(QLocalSocket *socket, quint64 requestId, const QString &message)
{
    send(socket, RuntimeHostMessage::Error, requestId,
         QCborMap{{QStringLiteral("message"), message}});
}

} // namespace LAStudio
