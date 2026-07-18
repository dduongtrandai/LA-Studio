#include "ApiServerService.h"

#include "core/Logger.h"
#include "core/Settings.h"
#include "controllers/stt/SttAudioDecoder.h"
#include "stt/SttEngine.h"
#include "stt/SttEngineInstance.h"
#include "tts/TtsEngine.h"
#include "tts/TtsEngineInstance.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>
#include <cstring>
#include <ctime>

namespace LAStudio {

namespace {

constexpr int kMaxBodyBytes = 50 * 1024 * 1024;
constexpr int kTtsTimeoutMs = 120000;
constexpr int kSttTimeoutMs = 120000;

QString statusText(int status)
{
    switch (status) {
    case 200: return QStringLiteral("OK");
    case 201: return QStringLiteral("Created");
    case 400: return QStringLiteral("Bad Request");
    case 401: return QStringLiteral("Unauthorized");
    case 404: return QStringLiteral("Not Found");
    case 409: return QStringLiteral("Conflict");
    case 415: return QStringLiteral("Unsupported Media Type");
    case 422: return QStringLiteral("Unprocessable Entity");
    case 429: return QStringLiteral("Too Many Requests");
    case 500: return QStringLiteral("Internal Server Error");
    case 503: return QStringLiteral("Service Unavailable");
    default: return QStringLiteral("OK");
    }
}

QByteArray toJsonBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString jsonString(const QJsonObject &object, const QString &key)
{
    const auto value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

QHash<QString, QString> parseHeaders(const QByteArray &headerBlock)
{
    QHash<QString, QString> headers;
    const QList<QByteArray> lines = headerBlock.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QString key = QString::fromUtf8(line.left(colon)).trimmed().toLower();
        const QString value = QString::fromUtf8(line.mid(colon + 1)).trimmed();
        headers.insert(key, value);
    }
    return headers;
}

QString headerValue(const QHash<QString, QString> &headers, const QString &name)
{
    return headers.value(name.trimmed().toLower());
}

QString pathFromTarget(const QString &target)
{
    const QUrl url(QStringLiteral("http://localhost") + target);
    return url.path().isEmpty() ? QStringLiteral("/") : url.path();
}

QString multipartBoundary(const QString &contentType)
{
    const int idx = contentType.toLower().indexOf(QStringLiteral("boundary="));
    if (idx < 0) {
        return {};
    }
    QString boundary = contentType.mid(idx + 9).trimmed();
    if (boundary.startsWith(QLatin1Char('"')) && boundary.endsWith(QLatin1Char('"')) && boundary.size() >= 2) {
        boundary = boundary.mid(1, boundary.size() - 2);
    }
    return boundary;
}

QHash<QString, QString> parseContentDisposition(const QString &value)
{
    QHash<QString, QString> out;
    const QStringList parts = value.split(QLatin1Char(';'));
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        const int eq = trimmed.indexOf(QLatin1Char('='));
        if (eq < 0) {
            continue;
        }
        QString key = trimmed.left(eq).trimmed().toLower();
        QString val = trimmed.mid(eq + 1).trimmed();
        if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
            val = val.mid(1, val.size() - 2);
        }
        out.insert(key, val);
    }
    return out;
}

QByteArray trimPart(const QByteArray &part)
{
    QByteArray out = part;
    while (out.startsWith("\r\n")) {
        out.remove(0, 2);
    }
    while (out.endsWith("\r\n")) {
        out.chop(2);
    }
    return out;
}

QByteArray buildWavBytes(const QByteArray &pcm16, int sampleRate, int channels)
{
    struct RiffHeader {
        char riff[4];
        quint32 chunkSize;
        char wave[4];
    };
    struct FmtChunk {
        char fmt[4];
        quint32 subchunkSize;
        quint16 audioFormat;
        quint16 numChannels;
        quint32 sampleRate;
        quint32 byteRate;
        quint16 blockAlign;
        quint16 bitsPerSample;
    };
    struct DataChunkHeader {
        char data[4];
        quint32 dataSize;
    };

    const quint32 dataSize = static_cast<quint32>(pcm16.size());

    RiffHeader riff;
    memcpy(riff.riff, "RIFF", 4);
    riff.chunkSize = 36 + dataSize;
    memcpy(riff.wave, "WAVE", 4);

    FmtChunk fmt;
    memcpy(fmt.fmt, "fmt ", 4);
    fmt.subchunkSize = 16;
    fmt.audioFormat = 1;
    fmt.numChannels = static_cast<quint16>(channels);
    fmt.sampleRate = static_cast<quint32>(sampleRate);
    fmt.bitsPerSample = 16;
    fmt.blockAlign = static_cast<quint16>(channels * 2);
    fmt.byteRate = fmt.sampleRate * fmt.blockAlign;

    DataChunkHeader data;
    memcpy(data.data, "data", 4);
    data.dataSize = dataSize;

    QByteArray out;
    out.reserve(sizeof(riff) + sizeof(fmt) + sizeof(data) + pcm16.size());
    out.append(reinterpret_cast<const char *>(&riff), sizeof(riff));
    out.append(reinterpret_cast<const char *>(&fmt), sizeof(fmt));
    out.append(reinterpret_cast<const char *>(&data), sizeof(data));
    out.append(pcm16);
    return out;
}

} // namespace

ApiServerService::ApiServerService(Settings *settings,
                                   TtsEngine *tts,
                                   SttEngine *stt,
                                   QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_tts(tts)
    , m_stt(stt)
{
    connect(&m_server, &QTcpServer::newConnection, this, &ApiServerService::onNewConnection);

    if (m_settings) {
        connect(m_settings, &Settings::apiServerEnabledChanged, this, &ApiServerService::syncFromSettings);
        connect(m_settings, &Settings::apiServerAllowLanChanged, this, &ApiServerService::syncFromSettings);
        connect(m_settings, &Settings::apiServerPortChanged, this, &ApiServerService::syncFromSettings);
        connect(m_settings, &Settings::apiServerApiKeyChanged, this, &ApiServerService::syncFromSettings);
    }

    syncFromSettings();
}

ApiServerService::~ApiServerService()
{
    stopServer();
}

bool ApiServerService::enabled() const
{
    return m_enabled;
}

void ApiServerService::setEnabled(bool value)
{
    if (m_enabled == value) {
        return;
    }
    m_enabled = value;
    emit enabledChanged();
    if (m_settings && m_settings->apiServerEnabled() != value) {
        m_settings->setApiServerEnabled(value);
    } else {
        applySettingsState();
    }
}

bool ApiServerService::running() const
{
    return m_running;
}

bool ApiServerService::allowLan() const
{
    return m_allowLan;
}

void ApiServerService::setAllowLan(bool value)
{
    if (m_allowLan == value) {
        return;
    }
    m_allowLan = value;
    emit allowLanChanged();
    if (m_settings && m_settings->apiServerAllowLan() != value) {
        m_settings->setApiServerAllowLan(value);
    } else {
        applySettingsState();
    }
}

int ApiServerService::port() const
{
    return m_port;
}

void ApiServerService::setPort(int value)
{
    value = qBound(1, value, 65535);
    if (m_port == value) {
        return;
    }
    m_port = value;
    emit portChanged();
    if (m_settings && m_settings->apiServerPort() != value) {
        m_settings->setApiServerPort(value);
    } else {
        applySettingsState();
    }
}

QString ApiServerService::apiKey() const
{
    return m_apiKey;
}

void ApiServerService::setApiKey(const QString &value)
{
    const QString normalized = value.trimmed();
    if (m_apiKey == normalized) {
        return;
    }
    m_apiKey = normalized;
    emit apiKeyChanged();
    if (m_settings && m_settings->apiServerApiKey() != normalized) {
        m_settings->setApiServerApiKey(normalized);
    }
}

QString ApiServerService::bindAddress() const
{
    if (!m_running) {
        return QString();
    }
    return m_allowLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1");
}

QString ApiServerService::baseUrl() const
{
    if (!m_running) {
        return QString();
    }
    return QStringLiteral("http://%1:%2").arg(bindAddress()).arg(m_port);
}

QString ApiServerService::lastError() const
{
    return m_lastError;
}

void ApiServerService::syncFromSettings()
{
    if (!m_settings) {
        return;
    }

    const bool nextEnabled = m_settings->apiServerEnabled();
    const bool nextAllowLan = m_settings->apiServerAllowLan();
    const int nextPort = qBound(1, m_settings->apiServerPort(), 65535);
    QString nextKey = m_settings->apiServerApiKey().trimmed();
    if (nextAllowLan && nextKey.isEmpty()) {
        nextKey = randomApiKey();
        m_settings->setApiServerApiKey(nextKey);
    }

    const bool changed = m_enabled != nextEnabled || m_allowLan != nextAllowLan || m_port != nextPort || m_apiKey != nextKey;
    m_enabled = nextEnabled;
    m_allowLan = nextAllowLan;
    m_port = nextPort;
    m_apiKey = nextKey;
    if (changed) {
        emit enabledChanged();
        emit allowLanChanged();
        emit portChanged();
        emit apiKeyChanged();
    }
    applySettingsState();
}

void ApiServerService::onNewConnection()
{
    while (auto *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &ApiServerService::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &ApiServerService::onSocketDisconnected);
    }
}

void ApiServerService::onSocketReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QByteArray &buffer = m_buffers[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > kMaxBodyBytes) {
        buffer.clear();
        HttpResponse response;
        response.status = 400;
        response.body = toJsonBytes(jsonErrorObject(QStringLiteral("Request body too large.")));
        writeResponse(socket, response);
        return;
    }

    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return;
    }

    const QByteArray headerBlock = buffer.left(headerEnd);
    const QHash<QString, QString> headers = parseHeaders(headerBlock);
    const int contentLength = headers.value(QStringLiteral("content-length")).toInt();
    if (buffer.size() < headerEnd + 4 + contentLength) {
        return;
    }

    HttpRequest request;
    const QString requestLine = QString::fromUtf8(headerBlock.split('\n').value(0)).trimmed();
    const QStringList parts = requestLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        HttpResponse response;
        response.status = 400;
        response.body = toJsonBytes(jsonErrorObject(QStringLiteral("Malformed request line.")));
        writeResponse(socket, response);
        return;
    }

    request.method = parts.at(0).trimmed().toUpper();
    request.target = parts.at(1).trimmed();
    request.path = pathFromTarget(request.target);
    request.peerAddress = socket->peerAddress();
    request.headers = headers;
    request.body = buffer.mid(headerEnd + 4, contentLength);

    if (headerValue(headers, QStringLiteral("content-type")).startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            request.jsonBody = doc.object();
        }
    }

    buffer.remove(0, headerEnd + 4 + contentLength);
    processRequestAsync(QPointer<QTcpSocket>(socket), request);
}

void ApiServerService::onSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }
    m_buffers.remove(socket);
    socket->deleteLater();
}

void ApiServerService::applySettingsState()
{
    if (!m_enabled) {
        stopServer();
        return;
    }
    if (m_running) {
        restartServer();
    } else {
        startServer();
    }
}

bool ApiServerService::startServer()
{
    if (m_running) {
        return true;
    }

    clearLastError();
    const QHostAddress address = m_allowLan ? QHostAddress::AnyIPv4 : QHostAddress::LocalHost;
    if (!m_server.listen(address, static_cast<quint16>(m_port))) {
        setLastError(QStringLiteral("Failed to start API server: %1").arg(m_server.errorString()));
        if (m_enabled) {
            m_enabled = false;
            emit enabledChanged();
            if (m_settings && m_settings->apiServerEnabled()) {
                m_settings->setApiServerEnabled(false);
            }
        }
        return false;
    }

    m_running = true;
    emit runningChanged();
    Logger::info("ApiServerService", QStringLiteral("API server listening on %1").arg(baseUrl()));
    return true;
}

void ApiServerService::stopServer()
{
    if (!m_running && !m_server.isListening()) {
        return;
    }
    m_server.close();
    m_running = false;
    emit runningChanged();
}

void ApiServerService::restartServer()
{
    stopServer();
    if (m_enabled) {
        startServer();
    }
}

void ApiServerService::setLastError(const QString &value)
{
    if (m_lastError == value) {
        return;
    }
    m_lastError = value;
    emit lastErrorChanged();
}

void ApiServerService::clearLastError()
{
    setLastError(QString());
}

QJsonObject ApiServerService::buildHealthDocument() const
{
    QJsonObject doc;
    doc.insert(QStringLiteral("status"), m_running ? QStringLiteral("ok") : QStringLiteral("stopped"));
    doc.insert(QStringLiteral("enabled"), m_enabled);
    doc.insert(QStringLiteral("running"), m_running);
    doc.insert(QStringLiteral("allow_lan"), m_allowLan);
    doc.insert(QStringLiteral("bind_address"), bindAddress());
    doc.insert(QStringLiteral("base_url"), baseUrl());
    doc.insert(QStringLiteral("port"), m_port);
    doc.insert(QStringLiteral("api_key_required"), m_allowLan && !m_apiKey.isEmpty());
    if (m_tts) {
        doc.insert(QStringLiteral("tts_loaded"), m_tts->isModelLoaded());
        doc.insert(QStringLiteral("tts_state"), static_cast<int>(m_tts->state()));
    }
    if (m_stt) {
        doc.insert(QStringLiteral("stt_loaded"), m_stt->isModelLoaded());
        doc.insert(QStringLiteral("stt_state"), static_cast<int>(m_stt->state()));
    }
    if (m_settings) {
        doc.insert(QStringLiteral("tts_family"), m_settings->selectedTtsFamily());
        doc.insert(QStringLiteral("stt_family"), m_settings->selectedSttFamily());
    }
    return doc;
}

QJsonObject ApiServerService::buildModelsDocument() const
{
    QJsonArray data;
    if (m_tts) {
        const QString active = m_tts->activeSignature();
        for (const QString &signature : m_tts->loadedSignatures()) {
            QJsonObject tts;
            tts.insert(QStringLiteral("id"), signature);
            tts.insert(QStringLiteral("object"), QStringLiteral("model"));
            tts.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
            tts.insert(QStringLiteral("purpose"), QStringLiteral("tts"));
            tts.insert(QStringLiteral("ready"), true);
            tts.insert(QStringLiteral("active"), signature == active);
            data.append(tts);
        }
    }
    if (m_stt) {
        const QString active = m_stt->activeSignature();
        for (const QString &signature : m_stt->loadedSignatures()) {
            QJsonObject stt;
            stt.insert(QStringLiteral("id"), signature);
            stt.insert(QStringLiteral("object"), QStringLiteral("model"));
            stt.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
            stt.insert(QStringLiteral("purpose"), QStringLiteral("stt"));
            stt.insert(QStringLiteral("ready"), true);
            stt.insert(QStringLiteral("active"), signature == active);
            data.append(stt);
        }
    }

    if (data.isEmpty() && m_settings) {
        QJsonObject tts;
        tts.insert(QStringLiteral("id"), QStringLiteral("tts"));
        tts.insert(QStringLiteral("object"), QStringLiteral("model"));
        tts.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
        tts.insert(QStringLiteral("purpose"), QStringLiteral("tts"));
        tts.insert(QStringLiteral("ready"), false);
        tts.insert(QStringLiteral("active"), false);
        data.append(tts);

        QJsonObject stt;
        stt.insert(QStringLiteral("id"), QStringLiteral("stt"));
        stt.insert(QStringLiteral("object"), QStringLiteral("model"));
        stt.insert(QStringLiteral("owned_by"), QStringLiteral("la-studio"));
        stt.insert(QStringLiteral("purpose"), QStringLiteral("stt"));
        stt.insert(QStringLiteral("ready"), false);
        stt.insert(QStringLiteral("active"), false);
        data.append(stt);
    }

    QJsonObject doc;
    doc.insert(QStringLiteral("object"), QStringLiteral("list"));
    doc.insert(QStringLiteral("data"), data);
    return doc;
}

QJsonObject ApiServerService::buildVoicesDocument() const
{
    QJsonArray data;
    for (auto it = m_customVoices.cbegin(); it != m_customVoices.cend(); ++it) {
        const CustomVoice &custom = it.value();
        QJsonObject voice;
        voice.insert(QStringLiteral("id"), custom.id);
        voice.insert(QStringLiteral("object"), QStringLiteral("audio.voice"));
        voice.insert(QStringLiteral("name"), custom.name);
        voice.insert(QStringLiteral("detail"), QStringLiteral("Custom voice from reference audio"));
        voice.insert(QStringLiteral("created_at"), custom.createdAt);
        data.append(voice);
    }

    if (m_tts) {
        const QVariantList schema = m_tts->currentSchema();
        for (const QVariant &var : schema) {
            const QVariantMap item = var.toMap();
            if (item.value(QStringLiteral("id")).toString() != QStringLiteral("voice")) {
                continue;
            }
            const QVariantList choices = item.value(QStringLiteral("choices")).toList();
            for (const QVariant &choiceVar : choices) {
                const QVariantMap choice = choiceVar.toMap();
                QJsonObject voice;
                voice.insert(QStringLiteral("id"), choice.value(QStringLiteral("value")).toString());
                voice.insert(QStringLiteral("name"), choice.value(QStringLiteral("text")).toString());
                voice.insert(QStringLiteral("detail"), choice.value(QStringLiteral("detail")).toString());
                data.append(voice);
            }
            break;
        }
    }

    if (data.isEmpty()) {
        QJsonObject voice;
        voice.insert(QStringLiteral("id"), QStringLiteral("default"));
        voice.insert(QStringLiteral("name"), QStringLiteral("default"));
        voice.insert(QStringLiteral("detail"), QStringLiteral("Current loaded TTS voice"));
        data.append(voice);
    }

    QJsonObject doc;
    doc.insert(QStringLiteral("object"), QStringLiteral("list"));
    doc.insert(QStringLiteral("data"), data);
    return doc;
}

bool ApiServerService::checkAuthorization(const HttpRequest &request) const
{
    if (m_apiKey.isEmpty()) {
        return true;
    }
    if (!m_allowLan || request.peerAddress.isLoopback() || request.peerAddress == QHostAddress::LocalHost || request.peerAddress == QHostAddress::LocalHostIPv6) {
        return true;
    }

    const QString auth = headerValue(request.headers, QStringLiteral("authorization"));
    if (auth.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive) && auth.mid(7).trimmed() == m_apiKey) {
        return true;
    }

    const QUrl url(QStringLiteral("http://localhost") + request.target);
    const QUrlQuery query(url);
    if (query.queryItemValue(QStringLiteral("api_key")) == m_apiKey) {
        return true;
    }

    return false;
}

QVariantMap ApiServerService::extraSettingsFromJson(const QJsonObject &json) const
{
    QVariantMap settings;
    for (auto it = json.begin(); it != json.end(); ++it) {
        const QString key = it.key();
        if (key == QStringLiteral("input") ||
            key == QStringLiteral("response_format") ||
            key == QStringLiteral("speed") ||
            key == QStringLiteral("model") ||
            key == QStringLiteral("voice")) {
            continue;
        }
        settings.insert(key, it.value().toVariant());
    }
    return settings;
}

QVariantMap ApiServerService::extraSettingsFromMultipart(const QHash<QString, QString> &fields) const
{
    QVariantMap settings;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        if (it.key() == QStringLiteral("file")) {
            continue;
        }
        settings.insert(it.key(), it.value());
    }
    return settings;
}

QVariantMap ApiServerService::currentVoiceSettings() const
{
    QVariantMap out;
    if (!m_tts) {
        return out;
    }
    const QVariantList schema = m_tts->currentSchema();
    for (const QVariant &itemVar : schema) {
        const QVariantMap item = itemVar.toMap();
        if (item.value(QStringLiteral("id")).toString() == QStringLiteral("voice")) {
            out = item;
            break;
        }
    }
    return out;
}

QVariantList ApiServerService::availableVoiceEntries() const
{
    QVariantList entries;
    const QVariantMap voiceSchema = currentVoiceSettings();
    const QVariantList choices = voiceSchema.value(QStringLiteral("choices")).toList();
    for (const QVariant &choiceVar : choices) {
        const QVariantMap choice = choiceVar.toMap();
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), choice.value(QStringLiteral("value")).toString());
        entry.insert(QStringLiteral("name"), choice.value(QStringLiteral("text")).toString());
        entry.insert(QStringLiteral("detail"), choice.value(QStringLiteral("detail")).toString());
        entries.append(entry);
    }
    if (entries.isEmpty()) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), QStringLiteral("default"));
        entry.insert(QStringLiteral("name"), QStringLiteral("default"));
        entry.insert(QStringLiteral("detail"), QStringLiteral("Current loaded TTS voice"));
        entries.append(entry);
    }
    return entries;
}

QVariantList ApiServerService::availableModelEntries() const
{
    QVariantList entries;
    if (!m_settings) {
        return entries;
    }
    QVariantMap tts;
    tts.insert(QStringLiteral("id"), m_settings->selectedTtsFamily().isEmpty() ? QStringLiteral("tts") : m_settings->selectedTtsFamily());
    tts.insert(QStringLiteral("kind"), QStringLiteral("tts"));
    tts.insert(QStringLiteral("family"), m_settings->selectedTtsFamily());
    tts.insert(QStringLiteral("runtime"), m_settings->selectedTtsRuntime());
    tts.insert(QStringLiteral("ready"), m_tts && m_tts->isModelLoaded());
    entries.append(tts);

    QVariantMap stt;
    stt.insert(QStringLiteral("id"), m_settings->selectedSttFamily().isEmpty() ? QStringLiteral("stt") : m_settings->selectedSttFamily());
    stt.insert(QStringLiteral("kind"), QStringLiteral("stt"));
    stt.insert(QStringLiteral("family"), m_settings->selectedSttFamily());
    stt.insert(QStringLiteral("runtime"), m_settings->selectedSttRuntime());
    stt.insert(QStringLiteral("ready"), m_stt && m_stt->isModelLoaded());
    entries.append(stt);

    return entries;
}

QJsonObject ApiServerService::parseJsonObject(const QByteArray &body, QString *error) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Invalid JSON body: %1").arg(parseError.errorString());
        }
        return {};
    }
    return doc.object();
}

QString ApiServerService::multipartBoundary(const QString &contentType)
{
    return ::LAStudio::multipartBoundary(contentType);
}

QByteArray ApiServerService::trimMultipartPart(const QByteArray &part)
{
    return trimPart(part);
}

QHash<QString, QString> ApiServerService::parseDispositionParams(const QString &value)
{
    return ::LAStudio::parseContentDisposition(value);
}

QHash<QString, QString> ApiServerService::parseHeaderLines(const QByteArray &headersBlob)
{
    return parseHeaders(headersBlob);
}

bool ApiServerService::parseMultipart(const QByteArray &body,
                                      const QString &contentType,
                                      QHash<QString, QString> *fields,
                                      QByteArray *fileData,
                                      QString *fileName,
                                      QString *mimeType,
                                      QString *error) const
{
    const QString boundaryString = multipartBoundary(contentType);
    if (boundaryString.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Missing multipart boundary.");
        }
        return false;
    }

    const QByteArray boundary = "--" + boundaryString.toUtf8();
    const QByteArray boundaryMarker = QByteArrayLiteral("\r\n") + boundary;

    int partStart = body.indexOf(boundary);
    if (partStart < 0) {
        if (error) {
            *error = QStringLiteral("Multipart boundary not found.");
        }
        return false;
    }
    partStart += boundary.size();

    while (partStart < body.size()) {
        if (body.mid(partStart, 2) == "--") {
            break;
        }
        if (body.mid(partStart, 2) == "\r\n") {
            partStart += 2;
        }

        const int nextBoundary = body.indexOf(boundaryMarker, partStart);
        QByteArray rawPart = nextBoundary >= 0 ? body.mid(partStart, nextBoundary - partStart) : body.mid(partStart);
        QByteArray part = trimPart(rawPart);
        if (part.isEmpty() || part == "--" || part.startsWith("--")) {
            if (nextBoundary < 0) {
                break;
            }
            partStart = nextBoundary + boundaryMarker.size();
            continue;
        }

        const int sep = part.indexOf("\r\n\r\n");
        if (sep < 0) {
            continue;
        }

        const QByteArray headerBlock = part.left(sep);
        QByteArray partBody = part.mid(sep + 4);
        partBody = trimPart(partBody);

        const QHash<QString, QString> headers = parseHeaders("X: dummy\r\n" + headerBlock);
        const QString disposition = headers.value(QStringLiteral("content-disposition"));
        if (disposition.isEmpty()) {
            continue;
        }

        const QHash<QString, QString> params = parseContentDisposition(disposition);
        const QString name = params.value(QStringLiteral("name"));
        const QString filename = params.value(QStringLiteral("filename"));
        if (!filename.isEmpty() || name == QStringLiteral("file")) {
            if (fileData) {
                *fileData = partBody;
            }
            if (fileName) {
                *fileName = filename.isEmpty() ? QStringLiteral("upload.wav") : filename;
            }
            if (mimeType) {
                *mimeType = headers.value(QStringLiteral("content-type"));
            }
        } else if (fields && !name.isEmpty()) {
            (*fields)[name] = QString::fromUtf8(partBody).trimmed();
        }

        if (nextBoundary < 0) {
            break;
        }
        partStart = nextBoundary + boundaryMarker.size();
    }

    return fileData && !fileData->isEmpty();
}

bool ApiServerService::parseHttpRequest(QByteArray &buffer, HttpRequest &request, int &consumed)
{
    consumed = 0;
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return false;
    }

    const QByteArray headerBlock = buffer.left(headerEnd);
    const QHash<QString, QString> headers = parseHeaders(headerBlock);
    const int contentLength = headers.value(QStringLiteral("content-length")).toInt();
    if (buffer.size() < headerEnd + 4 + contentLength) {
        return false;
    }

    const QString requestLine = QString::fromUtf8(headerBlock.split('\n').value(0)).trimmed();
    const QStringList parts = requestLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 2) {
        return false;
    }

    request.method = parts.at(0).trimmed().toUpper();
    request.target = parts.at(1).trimmed();
    request.path = pathFromTarget(request.target);
    request.headers = headers;
    request.body = buffer.mid(headerEnd + 4, contentLength);

    if (headerValue(headers, QStringLiteral("content-type")).startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(request.body, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            request.jsonBody = doc.object();
        }
    }

    consumed = headerEnd + 4 + contentLength;
    return true;
}

ApiServerService::HttpResponse ApiServerService::jsonResponse(const QJsonObject &object, int status) const
{
    HttpResponse response;
    response.status = status;
    response.body = toJsonBytes(object);
    response.contentType = QByteArrayLiteral("application/json; charset=utf-8");
    return response;
}

ApiServerService::HttpResponse ApiServerService::binaryResponse(const QByteArray &data, const QByteArray &contentType, int status) const
{
    HttpResponse response;
    response.status = status;
    response.body = data;
    response.contentType = contentType;
    return response;
}

ApiServerService::HttpResponse ApiServerService::errorResponse(int status, const QString &message, const QString &type) const
{
    return jsonResponse(jsonErrorObject(message, type), status);
}

ApiServerService::HttpResponse ApiServerService::unauthorizedResponse() const
{
    return errorResponse(401, QStringLiteral("API key required."), QStringLiteral("authentication_error"));
}

QJsonObject ApiServerService::jsonErrorObject(const QString &message, const QString &type)
{
    QJsonObject error;
    error.insert(QStringLiteral("message"), message);
    error.insert(QStringLiteral("type"), type);
    QJsonObject root;
    root.insert(QStringLiteral("error"), error);
    return root;
}

QString ApiServerService::guessContentType(const QString &format)
{
    const QString lowered = format.toLower();
    if (lowered == QStringLiteral("wav")) {
        return QStringLiteral("audio/wav");
    }
    if (lowered == QStringLiteral("pcm")) {
        return QStringLiteral("audio/pcm");
    }
    return QStringLiteral("application/octet-stream");
}

QString ApiServerService::randomApiKey()
{
    QString key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    key.remove(QLatin1Char('-'));
    return key;
}

QString ApiServerService::randomObjectId(const QString &prefix)
{
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    id.remove(QLatin1Char('-'));
    return prefix + QStringLiteral("_") + id.left(24);
}

void ApiServerService::writeResponse(QTcpSocket *socket, const HttpResponse &response)
{
    if (!socket) {
        return;
    }

    QByteArray out;
    out += "HTTP/1.1 ";
    out += QByteArray::number(response.status);
    out += ' ';
    out += statusText(response.status).toUtf8();
    out += "\r\nConnection: close\r\n";
    out += "Content-Type: ";
    out += response.contentType;
    out += "\r\nContent-Length: ";
    out += QByteArray::number(response.body.size());
    out += "\r\n";
    for (const auto &header : response.headers) {
        out += header.first;
        out += ": ";
        out += header.second;
        out += "\r\n";
    }
    out += "\r\n";
    out += response.body;
    socket->write(out);
    socket->disconnectFromHost();
}

void ApiServerService::processRequestAsync(const QPointer<QTcpSocket> &socket, const HttpRequest &request)
{
    QtConcurrent::run([this, socket, request]() {
        HttpResponse response = handleRequest(request);
        QMetaObject::invokeMethod(this, [this, socket, response]() {
            if (socket) {
                writeResponse(socket, response);
            }
        }, Qt::QueuedConnection);
    });
}

ApiServerService::HttpResponse ApiServerService::handleRequest(const HttpRequest &request)
{
    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/health")) {
        return jsonResponse(buildHealthDocument());
    }

    if (!checkAuthorization(request)) {
        return unauthorizedResponse();
    }

    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/v1/models")) {
        return jsonResponse(buildModelsDocument());
    }
    if (request.method == QStringLiteral("GET") && request.path == QStringLiteral("/v1/audio/voices")) {
        return jsonResponse(buildVoicesDocument());
    }
    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/v1/audio/voice_consents")) {
        return handleCreateVoiceConsentRequest(request);
    }
    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/v1/audio/voices")) {
        return handleCreateVoiceRequest(request);
    }
    if (request.method == QStringLiteral("POST") &&
        (request.path == QStringLiteral("/v1/audio/speech") ||
         request.path == QStringLiteral("/v1/tts/speech")))
    {
        return handleSpeechRequest(request);
    }
    if (request.method == QStringLiteral("POST") &&
        (request.path == QStringLiteral("/v1/audio/transcriptions") ||
         request.path == QStringLiteral("/v1/stt/transcriptions")))
    {
        return handleTranscriptionRequest(request);
    }
    if (request.method == QStringLiteral("POST") && request.path == QStringLiteral("/v1/audio/voice_designs")) {
        return handleVoiceDesignRequest(request);
    }

    return errorResponse(404, QStringLiteral("Route not found: %1").arg(request.path), QStringLiteral("not_found"));
}

ApiServerService::HttpResponse ApiServerService::runTtsGeneration(const QString &input,
                                                                  const QString &format,
                                                                  const QVariantMap &settings,
                                                                  float speed,
                                                                  const QString &mode,
                                                                  const QString &referencePath,
                                                                  const QString &modelId)
{
    if (!m_tts) {
        return errorResponse(503, QStringLiteral("TTS service is not available."));
    }
    if (!m_tts->isModelLoaded()) {
        return errorResponse(409, QStringLiteral("Load a TTS model in the app before calling /v1/audio/speech."));
    }

    const QString targetModelId = modelId.isEmpty() ? m_tts->activeSignature() : modelId;
    TtsEngineInstance *target = m_tts->instance(targetModelId);
    if (!target) {
        return errorResponse(404, QStringLiteral("Requested TTS model is not loaded: %1").arg(targetModelId), QStringLiteral("not_found"));
    }
    if (target->isProcessing()) {
        return errorResponse(409, QStringLiteral("Requested TTS model is busy: %1").arg(targetModelId));
    }

    if (input.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: input"));
    }
    if (format != QStringLiteral("wav") && format != QStringLiteral("pcm")) {
        return errorResponse(415, QStringLiteral("This build currently supports response_format=wav or pcm."));
    }

    const QString generationMode = mode.isEmpty() ? QStringLiteral("speech") : mode;
    if (generationMode == QStringLiteral("voice-cloning") && referencePath.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing reference audio for voice cloning."));
    }

    QEventLoop loop;
    struct Result {
        bool ok = false;
        QByteArray pcm16;
        int sampleRate = 24000;
        QString error;
    } result;

    QMetaObject::Connection okConn = connect(target, &TtsEngineInstance::synthesisFinished, &loop,
                                             [&](const QByteArray &pcm16, int sampleRate) {
        result.ok = true;
        result.pcm16 = pcm16;
        result.sampleRate = sampleRate;
        loop.quit();
    });
    QMetaObject::Connection errConn = connect(target, &TtsEngineInstance::errorOccurred, &loop,
                                              [&](const QString &msg) {
        result.ok = false;
        result.error = msg;
        loop.quit();
    });
    QTimer timeout;
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, &loop, [&]() {
        result.ok = false;
        result.error = QStringLiteral("TTS request timed out.");
        loop.quit();
    });

    if (generationMode == QStringLiteral("voice-cloning")) {
        QMetaObject::invokeMethod(target, [target, input, referencePath, settings]() {
            target->cloneVoice(input, referencePath, settings);
        }, Qt::QueuedConnection);
    } else if (generationMode == QStringLiteral("voice-design")) {
        QMetaObject::invokeMethod(target, [target, input, settings]() {
            target->designVoice(input, settings);
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(target, [target, input, speed, settings]() {
            target->synthesize(input, 0, speed, settings);
        }, Qt::QueuedConnection);
    }

    timeout.start(kTtsTimeoutMs);
    loop.exec();

    disconnect(okConn);
    disconnect(errConn);

    if (!result.ok) {
        return errorResponse(500, result.error.isEmpty() ? QStringLiteral("TTS synthesis failed.") : result.error);
    }

    const QByteArray body = format == QStringLiteral("pcm")
        ? result.pcm16
        : buildWavBytes(result.pcm16, result.sampleRate, 1);
    return binaryResponse(body, guessContentType(format).toUtf8());
}

ApiServerService::HttpResponse ApiServerService::handleSpeechRequest(const HttpRequest &request)
{
    QJsonObject json = request.jsonBody;
    if (json.isEmpty()) {
        QString error;
        json = parseJsonObject(request.body, &error);
        if (!error.isEmpty()) {
            return errorResponse(400, error);
        }
    }

    const QString input = jsonString(json, QStringLiteral("input")).trimmed();
    if (input.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: input"));
    }

    const QString format = jsonString(json, QStringLiteral("response_format")).isEmpty()
        ? QStringLiteral("wav")
        : jsonString(json, QStringLiteral("response_format")).toLower();
    if (format != QStringLiteral("wav") && format != QStringLiteral("pcm")) {
        return errorResponse(415, QStringLiteral("This build currently supports response_format=wav or pcm."));
    }

    QVariantMap settings = extraSettingsFromJson(json);
    const QString voice = jsonString(json, QStringLiteral("voice"));
    QString mode = QStringLiteral("speech");
    QString referencePath;
    if (!voice.isEmpty() && m_customVoices.contains(voice)) {
        const CustomVoice custom = m_customVoices.value(voice);
        settings.insert(QStringLiteral("voice"), custom.name);
        mode = QStringLiteral("voice-cloning");
        referencePath = custom.samplePath;
    } else if (!voice.isEmpty()) {
        settings.insert(QStringLiteral("voice"), voice);
    }
    const QString language = jsonString(json, QStringLiteral("language"));
    if (!language.isEmpty()) {
        settings.insert(QStringLiteral("lang"), language);
    }
    const QString instruct = jsonString(json, QStringLiteral("instruct"));
    if (!instruct.isEmpty()) {
        settings.insert(QStringLiteral("instruct"), instruct);
    }
    if (json.contains(QStringLiteral("seed"))) {
        settings.insert(QStringLiteral("seed"), json.value(QStringLiteral("seed")).toVariant());
    }
    const float speed = static_cast<float>(json.value(QStringLiteral("speed")).toDouble(1.0));
    const QString modelId = jsonString(json, QStringLiteral("model"));

    return runTtsGeneration(input, format, settings, speed, mode, referencePath, modelId);
}

ApiServerService::HttpResponse ApiServerService::handleCreateVoiceConsentRequest(const HttpRequest &request)
{
    QHash<QString, QString> fields;
    QByteArray recordingBytes;
    QString fileName;
    QString mimeType;
    QString error;

    const QString contentType = headerValue(request.headers, QStringLiteral("content-type"));
    if (contentType.startsWith(QStringLiteral("multipart/form-data"), Qt::CaseInsensitive)) {
        if (!parseMultipart(request.body, contentType, &fields, &recordingBytes, &fileName, &mimeType, &error)) {
            return errorResponse(400, error.isEmpty() ? QStringLiteral("Invalid multipart form-data.") : error);
        }
    } else if (contentType.startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonObject json = request.jsonBody;
        if (json.isEmpty()) {
            json = parseJsonObject(request.body, &error);
            if (!error.isEmpty()) {
                return errorResponse(400, error);
            }
        }
        fields.insert(QStringLiteral("name"), jsonString(json, QStringLiteral("name")));
        fields.insert(QStringLiteral("language"), jsonString(json, QStringLiteral("language")));
        const QString recordingBase64 = jsonString(json, QStringLiteral("recording_base64"));
        if (!recordingBase64.isEmpty()) {
            recordingBytes = QByteArray::fromBase64(recordingBase64.toUtf8());
            fileName = QStringLiteral("consent.wav");
        }
    } else {
        return errorResponse(415, QStringLiteral("This endpoint expects multipart/form-data or application/json."));
    }

    if (recordingBytes.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: recording."));
    }

    const QString id = randomObjectId(QStringLiteral("cons"));
    QJsonObject doc;
    doc.insert(QStringLiteral("id"), id);
    doc.insert(QStringLiteral("object"), QStringLiteral("audio.voice_consent"));
    doc.insert(QStringLiteral("created_at"), static_cast<qint64>(std::time(nullptr)));
    doc.insert(QStringLiteral("language"), fields.value(QStringLiteral("language"), QStringLiteral("und")));
    doc.insert(QStringLiteral("name"), fields.value(QStringLiteral("name"), fileName.isEmpty() ? QStringLiteral("Voice consent") : fileName));
    m_voiceConsents.insert(id, doc);
    return jsonResponse(doc, 201);
}

ApiServerService::HttpResponse ApiServerService::handleCreateVoiceRequest(const HttpRequest &request)
{
    QHash<QString, QString> fields;
    QByteArray sampleBytes;
    QString fileName;
    QString mimeType;
    QString error;

    const QString contentType = headerValue(request.headers, QStringLiteral("content-type"));
    if (contentType.startsWith(QStringLiteral("multipart/form-data"), Qt::CaseInsensitive)) {
        if (!parseMultipart(request.body, contentType, &fields, &sampleBytes, &fileName, &mimeType, &error)) {
            return errorResponse(400, error.isEmpty() ? QStringLiteral("Invalid multipart form-data.") : error);
        }
    } else if (contentType.startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonObject json = request.jsonBody;
        if (json.isEmpty()) {
            json = parseJsonObject(request.body, &error);
            if (!error.isEmpty()) {
                return errorResponse(400, error);
            }
        }
        fields.insert(QStringLiteral("name"), jsonString(json, QStringLiteral("name")));
        fields.insert(QStringLiteral("consent"), jsonString(json, QStringLiteral("consent")));
        const QString audioBase64 = jsonString(json, QStringLiteral("audio_sample_base64"));
        if (!audioBase64.isEmpty()) {
            sampleBytes = QByteArray::fromBase64(audioBase64.toUtf8());
            fileName = QStringLiteral("voice.wav");
        }
    } else {
        return errorResponse(415, QStringLiteral("This endpoint expects multipart/form-data or application/json."));
    }

    if (sampleBytes.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: audio_sample."));
    }

    const QString suffix = QFileInfo(fileName).suffix().isEmpty()
        ? QStringLiteral("wav")
        : QFileInfo(fileName).suffix();
    QTemporaryFile temp(QDir::tempPath() + QStringLiteral("/lastudio-api-voice-XXXXXX.") + suffix);
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return errorResponse(500, QStringLiteral("Failed to store custom voice audio."));
    }
    temp.write(sampleBytes);
    temp.flush();
    const QString samplePath = temp.fileName();
    temp.close();

    CustomVoice voice;
    voice.id = randomObjectId(QStringLiteral("voice"));
    voice.name = fields.value(QStringLiteral("name"), fileName.isEmpty() ? voice.id : QFileInfo(fileName).completeBaseName());
    voice.consentId = fields.value(QStringLiteral("consent"));
    voice.samplePath = samplePath;
    voice.fileName = fileName;
    voice.mimeType = mimeType;
    voice.createdAt = static_cast<qint64>(std::time(nullptr));
    m_customVoices.insert(voice.id, voice);

    QJsonObject doc;
    doc.insert(QStringLiteral("id"), voice.id);
    doc.insert(QStringLiteral("object"), QStringLiteral("audio.voice"));
    doc.insert(QStringLiteral("created_at"), voice.createdAt);
    doc.insert(QStringLiteral("name"), voice.name);
    return jsonResponse(doc, 201);
}

ApiServerService::HttpResponse ApiServerService::handleVoiceDesignRequest(const HttpRequest &request)
{
    QJsonObject json = request.jsonBody;
    if (json.isEmpty()) {
        QString error;
        json = parseJsonObject(request.body, &error);
        if (!error.isEmpty()) {
            return errorResponse(400, error);
        }
    }

    const QString input = jsonString(json, QStringLiteral("input")).trimmed();
    if (input.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing required field: input"));
    }

    const QString format = jsonString(json, QStringLiteral("response_format")).isEmpty()
        ? QStringLiteral("wav")
        : jsonString(json, QStringLiteral("response_format")).toLower();
    QVariantMap settings = extraSettingsFromJson(json);
    const QString voiceDescription = jsonString(json, QStringLiteral("voice_description"));
    if (!voiceDescription.isEmpty()) {
        settings.insert(QStringLiteral("voice_description"), voiceDescription);
        settings.insert(QStringLiteral("description"), voiceDescription);
    }
    const QString language = jsonString(json, QStringLiteral("language"));
    if (!language.isEmpty()) {
        settings.insert(QStringLiteral("lang"), language);
    }

    const QString modelId = jsonString(json, QStringLiteral("model"));
    return runTtsGeneration(input, format, settings, 1.0f, QStringLiteral("voice-design"), QString(), modelId);
}

ApiServerService::HttpResponse ApiServerService::handleTranscriptionRequest(const HttpRequest &request)
{
    if (!m_stt) {
        return errorResponse(503, QStringLiteral("STT service is not available."));
    }
    if (!m_stt->isModelLoaded()) {
        return errorResponse(409, QStringLiteral("Load an STT model in the app before calling /v1/audio/transcriptions."));
    }

    QByteArray fileBytes;
    QString fileName;
    QString mimeType;
    QHash<QString, QString> fields;
    QString error;

    const QString contentType = headerValue(request.headers, QStringLiteral("content-type"));
    if (contentType.startsWith(QStringLiteral("multipart/form-data"), Qt::CaseInsensitive)) {
        if (!parseMultipart(request.body, contentType, &fields, &fileBytes, &fileName, &mimeType, &error)) {
            return errorResponse(400, error.isEmpty() ? QStringLiteral("Invalid multipart form-data.") : error);
        }
    } else if (contentType.startsWith(QStringLiteral("application/json"), Qt::CaseInsensitive)) {
        QJsonObject json = request.jsonBody;
        if (json.isEmpty()) {
            json = parseJsonObject(request.body, &error);
            if (!error.isEmpty()) {
                return errorResponse(400, error);
            }
        }
        const QString audioBase64 = jsonString(json, QStringLiteral("audio_base64"));
        if (audioBase64.isEmpty()) {
            return errorResponse(422, QStringLiteral("Expected multipart/form-data or JSON with audio_base64."));
        }
        fileBytes = QByteArray::fromBase64(audioBase64.toUtf8());
        fileName = QStringLiteral("upload.wav");
        fields.insert(QStringLiteral("language"), jsonString(json, QStringLiteral("language")));
        fields.insert(QStringLiteral("model"), jsonString(json, QStringLiteral("model")));
        fields.insert(QStringLiteral("translate"), json.value(QStringLiteral("translate")).toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        if (json.contains(QStringLiteral("threads"))) {
            fields.insert(QStringLiteral("threads"), QString::number(json.value(QStringLiteral("threads")).toInt(0)));
        }
    } else {
        return errorResponse(415, QStringLiteral("This endpoint expects multipart/form-data or application/json."));
    }

    if (fileBytes.isEmpty()) {
        return errorResponse(422, QStringLiteral("Missing uploaded audio file."));
    }

    const QString language = fields.value(QStringLiteral("language"), QStringLiteral("en"));
    const QString modelId = fields.value(QStringLiteral("model")).trimmed();
    const bool translate = fields.value(QStringLiteral("translate")).toLower() == QStringLiteral("true");
    const int threads = qBound(0, fields.value(QStringLiteral("threads"), QStringLiteral("0")).toInt(), 64);

    const QString targetModelId = modelId.isEmpty() ? m_stt->activeSignature() : modelId;
    SttEngineInstance *target = m_stt->instance(targetModelId);
    if (!target) {
        return errorResponse(404, QStringLiteral("Requested STT model is not loaded: %1").arg(targetModelId), QStringLiteral("not_found"));
    }
    if (target->isProcessing()) {
        return errorResponse(409, QStringLiteral("Requested STT model is busy: %1").arg(targetModelId));
    }

    QTemporaryFile temp(QDir::tempPath() + QStringLiteral("/lastudio-api-XXXXXX") +
                        (fileName.isEmpty() ? QStringLiteral(".wav") : QStringLiteral(".") + QFileInfo(fileName).suffix()));
    temp.setAutoRemove(false);
    if (!temp.open()) {
        return errorResponse(500, QStringLiteral("Failed to create a temporary upload file."));
    }
    temp.write(fileBytes);
    temp.flush();
    const QString tempPath = temp.fileName();
    temp.close();

    QEventLoop loop;
    struct DecodeResult {
        bool ok = false;
        QVector<float> samples;
        QString error;
    } decodeResult;

    SttAudioDecoder decoder;
    QMetaObject::Connection decodeConn = connect(&decoder, &SttAudioDecoder::finished, &loop,
                                                 [&](const QVector<float> &samples) {
        decodeResult.ok = true;
        decodeResult.samples = samples;
        loop.quit();
    });
    QMetaObject::Connection decodeErrConn = connect(&decoder, &SttAudioDecoder::errorOccurred, &loop,
                                                     [&](const QString &msg) {
        decodeResult.ok = false;
        decodeResult.error = msg;
        loop.quit();
    });
    QTimer decodeTimeout;
    decodeTimeout.setSingleShot(true);
    connect(&decodeTimeout, &QTimer::timeout, &loop, [&]() {
        decodeResult.ok = false;
        decodeResult.error = QStringLiteral("Audio decode timed out.");
        loop.quit();
    });

    decoder.startDecode(tempPath);
    decodeTimeout.start(kSttTimeoutMs);
    loop.exec();

    disconnect(decodeConn);
    disconnect(decodeErrConn);

    if (!decodeResult.ok) {
        QFile::remove(tempPath);
        return errorResponse(500, decodeResult.error.isEmpty() ? QStringLiteral("Audio decode failed.") : decodeResult.error);
    }

    QEventLoop transcribeLoop;
    struct TranscribeResult {
        bool ok = false;
        QString text;
        QString error;
    } transcribeResult;

    QMetaObject::Connection transcribeConn = connect(target, &SttEngineInstance::transcriptionFinished, &transcribeLoop,
                                                      [&](const QString &text, const QVariantList &) {
        transcribeResult.ok = true;
        transcribeResult.text = text;
        transcribeLoop.quit();
    });
    QMetaObject::Connection transcribeErrConn = connect(target, &SttEngineInstance::errorOccurred, &transcribeLoop,
                                                        [&](const QString &msg) {
        transcribeResult.ok = false;
        transcribeResult.error = msg;
        transcribeLoop.quit();
    });
    QTimer transcribeTimeout;
    transcribeTimeout.setSingleShot(true);
    connect(&transcribeTimeout, &QTimer::timeout, &transcribeLoop, [&]() {
        transcribeResult.ok = false;
        transcribeResult.error = QStringLiteral("Transcription request timed out.");
        transcribeLoop.quit();
    });

    QMetaObject::invokeMethod(target, [target, decodeResult, language, threads, translate]() {
        target->transcribeSamples(decodeResult.samples, language, threads, translate, QVariantMap());
    }, Qt::QueuedConnection);
    transcribeTimeout.start(kSttTimeoutMs);
    transcribeLoop.exec();

    disconnect(transcribeConn);
    disconnect(transcribeErrConn);
    QFile::remove(tempPath);

    if (!transcribeResult.ok) {
        return errorResponse(500, transcribeResult.error.isEmpty() ? QStringLiteral("Transcription failed.") : transcribeResult.error);
    }

    QJsonObject doc;
    doc.insert(QStringLiteral("text"), transcribeResult.text);
    return jsonResponse(doc);
}

} // namespace LAStudio
