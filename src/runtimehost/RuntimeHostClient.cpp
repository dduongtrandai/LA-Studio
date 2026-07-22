#include "RuntimeHostClient.h"

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QThread>

namespace LAStudio {

RuntimeHostClient::RuntimeHostClient(QObject *parent)
    : QObject(parent)
{
}

RuntimeHostClient::~RuntimeHostClient()
{
    QString ignored;
    shutdown(&ignored);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(2000);
    }
}

bool RuntimeHostClient::start(const QString &hostExecutable, QString *error)
{
    if (isRunning()) return true;
    if (hostExecutable.isEmpty()) {
        if (error) *error = QStringLiteral("RuntimeHost executable path is empty.");
        return false;
    }

    // A previous host may have exited unexpectedly.  Reset the old socket
    // state before reusing this client for a reload; otherwise QLocalSocket
    // can remain in ClosingState and reject the new endpoint connection.
    if (m_socket.state() != QLocalSocket::UnconnectedState) {
        m_socket.abort();
    }
    m_socket.close();

    const quint32 nonce = QRandomGenerator::global()->generate();
    // Windows named-pipe names reject the dash-separated form on some Qt
    // builds; keep the endpoint to a conservative alphanumeric/underscore
    // name that is valid on Windows and Unix.
    m_socketName = QStringLiteral("lastudio_runtime_%1_%2")
                       .arg(QCoreApplication::applicationPid())
                       .arg(nonce, 8, 16, QLatin1Char('0'));
    m_token = QStringLiteral("%1-%2")
                  .arg(QRandomGenerator::global()->generate64(), 16, 16, QLatin1Char('0'))
                  .arg(QRandomGenerator::global()->generate64(), 16, 16, QLatin1Char('0'));

    m_process.setProgram(hostExecutable);
    m_process.setArguments({QStringLiteral("--socket"), m_socketName,
                             QStringLiteral("--token"), m_token});
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    if (!m_process.waitForStarted(10000)) {
        if (error) *error = QStringLiteral("Could not start RuntimeHost: %1").arg(m_process.errorString());
        return false;
    }
    if (!connectSocket(&m_socket, error) || !authenticate(&m_socket, error)) {
        m_process.kill();
        m_process.waitForFinished(2000);
        return false;
    }
    return true;
}

bool RuntimeHostClient::isRunning() const
{
    return m_process.state() == QProcess::Running && m_socket.state() == QLocalSocket::ConnectedState;
}

bool RuntimeHostClient::ping(QString *error)
{
    RuntimeHostFrame response;
    return request(RuntimeHostMessage::Ping, QCborMap{}, &response, error)
        && response.message == RuntimeHostMessage::Pong;
}

bool RuntimeHostClient::shutdown(QString *error)
{
    if (m_process.state() == QProcess::NotRunning) {
        m_socket.abort();
        m_socket.close();
        return true;
    }
    RuntimeHostFrame response;
    const bool ok = request(RuntimeHostMessage::Shutdown, QCborMap{}, &response, error);
    m_socket.disconnectFromServer();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.waitForFinished(2000);
    }
    return ok;
}

bool RuntimeHostClient::load(const QCborMap &configuration, QCborValue *schema, QString *error)
{
    RuntimeHostFrame response;
    if (!request(RuntimeHostMessage::Load, configuration, &response, error)) return false;
    QCborMap payload;
    QString decodeError;
    if (!decodeRuntimeHostCbor(response.payload, &payload, &decodeError)) {
        if (error) *error = decodeError;
        return false;
    }
    if (schema) *schema = payload.value(QStringLiteral("schema"));
    return payload.value(QStringLiteral("ok")).toBool();
}

bool RuntimeHostClient::infer(const QCborMap &requestPayload,
                              const QVector<float> &referenceSamples,
                              QVector<float> *samples,
                              int *sampleRate,
                              QString *error)
{
    if (!samples || !sampleRate) {
        if (error) *error = QStringLiteral("RuntimeHost output destination is null.");
        return false;
    }
    QCborMap result;
    return execute(requestPayload, referenceSamples, &result, samples, sampleRate, error, 24000);
}

bool RuntimeHostClient::execute(const QCborMap &requestPayload,
                                const QVector<float> &referenceSamples,
                                QCborMap *resultPayload,
                                QVector<float> *samples,
                                int *sampleRate,
                                QString *error,
                                int inputSampleRate)
{
    if (!resultPayload) {
        if (error) *error = QStringLiteral("RuntimeHost result destination is null.");
        return false;
    }
    RuntimeHostSharedBuffer input;
    QCborMap payload = requestPayload;
    if (!referenceSamples.isEmpty()) {
        QCborMap descriptor;
        if (!input.createFromSamples(referenceSamples, inputSampleRate, 1, &descriptor, error)) return false;
        payload.insert(QStringLiteral("referenceBuffer"), descriptor);
    }
    RuntimeHostFrame response;
    if (!request(RuntimeHostMessage::Infer, payload, &response, error)) return false;
    QCborMap result;
    QString decodeError;
    if (!decodeRuntimeHostCbor(response.payload, &result, &decodeError)) {
        if (error) *error = decodeError;
        return false;
    }
    if (samples || sampleRate) {
        if (!samples || !sampleRate) {
            if (error) *error = QStringLiteral("RuntimeHost audio output destinations must be provided together.");
            return false;
        }
        const QCborValue descriptor = result.value(QStringLiteral("outputBuffer"));
        RuntimeHostSharedBuffer output;
        if (!descriptor.isMap() || !output.attach(descriptor.toMap(), &decodeError)
            || !output.copyTo(samples, &decodeError)) {
            if (error) *error = decodeError;
            return false;
        }
        *sampleRate = result.value(QStringLiteral("sampleRate")).toInteger();
        if (samples->isEmpty() || *sampleRate <= 0) {
            if (error) *error = QStringLiteral("RuntimeHost returned invalid audio output.");
            return false;
        }
    }
    *resultPayload = result;
    return true;
}

bool RuntimeHostClient::request(RuntimeHostMessage message,
                                const QCborMap &payload,
                                RuntimeHostFrame *response,
                                QString *error)
{
    if (!response) {
        if (error) *error = QStringLiteral("RuntimeHost response destination is null.");
        return false;
    }
    if (!isRunning()) {
        if (error) *error = QStringLiteral("RuntimeHost is not running.");
        return false;
    }

    const quint64 requestId = m_nextRequestId.fetch_add(1, std::memory_order_relaxed);
    m_currentRequestId.store(requestId, std::memory_order_release);
    const bool sent = sendFrame(&m_socket, message, requestId, payload, error);
    if (!sent) {
        m_currentRequestId.store(0, std::memory_order_release);
        return false;
    }
    const bool received = readResponse(&m_socket, requestId, response, error);
    m_currentRequestId.store(0, std::memory_order_release);
    return received;
}

void RuntimeHostClient::cancelCurrent()
{
    const quint64 requestId = m_currentRequestId.load(std::memory_order_acquire);
    if (m_process.state() != QProcess::Running || requestId == 0) return;

    // Cancellation can be called from the UI thread while the model worker is
    // synchronously waiting for its response. Use a short-lived control
    // connection instead of touching the worker-owned socket from another
    // thread.
    QLocalSocket control;
    QString ignored;
    if (!connectSocket(&control, &ignored) || !authenticate(&control, &ignored)) return;
    sendFrame(&control, RuntimeHostMessage::Cancel, requestId, {}, &ignored);
    control.disconnectFromServer();
}

bool RuntimeHostClient::connectSocket(QLocalSocket *socket, QString *error)
{
    if (!socket) return false;
    QString lastError;
    // The host process may have started before QLocalServer::listen() has
    // completed. Retry the endpoint during that short startup window instead
    // of treating a transient Windows named-pipe error as a hard failure.
    for (int attempt = 0; attempt < 100; ++attempt) {
        socket->connectToServer(m_socketName);
        if (socket->waitForConnected(100)) return true;
        lastError = socket->errorString();
        socket->abort();
        QThread::msleep(50);
    }
    if (error) *error = QStringLiteral("Could not connect to RuntimeHost: %1").arg(lastError);
    return false;
}

bool RuntimeHostClient::authenticate(QLocalSocket *socket, QString *error)
{
    const quint64 requestId = m_nextRequestId.fetch_add(1, std::memory_order_relaxed);
    if (!sendFrame(socket, RuntimeHostMessage::Hello, requestId,
                   QCborMap{{QStringLiteral("token"), m_token},
                            {QStringLiteral("protocolMajor"), kRuntimeHostProtocolMajor}},
                   error)) {
        return false;
    }
    RuntimeHostFrame response;
    if (!readResponse(socket, requestId, &response, error)) return false;
    if (response.message != RuntimeHostMessage::HelloAck) {
        if (error) *error = protocolError(response);
        return false;
    }
    return true;
}

bool RuntimeHostClient::sendFrame(QLocalSocket *socket,
                                  RuntimeHostMessage message,
                                  quint64 requestId,
                                  const QCborMap &payload,
                                  QString *error)
{
    if (!socket || socket->state() != QLocalSocket::ConnectedState) {
        if (error) *error = QStringLiteral("RuntimeHost socket is not connected.");
        return false;
    }
    socket->write(encodeRuntimeHostFrame(message, requestId, encodeRuntimeHostCbor(payload)));
    if (!socket->waitForBytesWritten(5000)) {
        if (error) *error = socket->errorString();
        return false;
    }
    return true;
}

bool RuntimeHostClient::readResponse(QLocalSocket *socket,
                                     quint64 requestId,
                                     RuntimeHostFrame *response,
                                     QString *error)
{
    RuntimeHostFrameParser parser;
    while (true) {
        QString parseError;
        while (const auto frame = parser.takeNext(&parseError)) {
            if (frame->message == RuntimeHostMessage::Progress) {
                QCborMap progress;
                if (decodeRuntimeHostCbor(frame->payload, &progress, &parseError)
                    && m_progressCallback) {
                    m_progressCallback(progress);
                }
                continue;
            }
            if (!parseError.isEmpty()) {
                if (error) *error = parseError;
                return false;
            }
            if (frame->requestId != requestId) continue;
            *response = *frame;
            if (frame->message == RuntimeHostMessage::Error) {
                if (error) *error = protocolError(*frame);
                return false;
            }
            return true;
        }
        if (!parseError.isEmpty()) {
            if (error) *error = parseError;
            return false;
        }
        if (!socket->waitForReadyRead(10000)) {
            if (error) {
                if (m_process.state() == QProcess::NotRunning) {
                    *error = QStringLiteral("RuntimeHost exited unexpectedly (code %1). Reload the model to restart its isolated runtime.")
                                  .arg(m_process.exitCode());
                } else {
                    *error = QStringLiteral("RuntimeHost response timed out: %1").arg(socket->errorString());
                }
            }
            return false;
        }
        parser.append(socket->readAll());
    }
}

QString RuntimeHostClient::protocolError(const RuntimeHostFrame &frame) const
{
    QCborMap payload;
    QString ignored;
    if (decodeRuntimeHostCbor(frame.payload, &payload, &ignored)) {
        const QString message = payload.value(QStringLiteral("message")).toString();
        if (!message.isEmpty()) return message;
    }
    return QStringLiteral("RuntimeHost returned an invalid error response.");
}

} // namespace LAStudio
