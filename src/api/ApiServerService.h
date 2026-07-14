#pragma once

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtQml/qqml.h>

namespace LAStudio {

class Settings;
class TtsEngine;
class SttEngine;

class ApiServerService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ApiServerService is managed by AppController")

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool allowLan READ allowLan WRITE setAllowLan NOTIFY allowLanChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString bindAddress READ bindAddress NOTIFY runningChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl NOTIFY runningChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ApiServerService(Settings *settings,
                              TtsEngine *tts,
                              SttEngine *stt,
                              QObject *parent = nullptr);
    ~ApiServerService() override;

    bool enabled() const;
    void setEnabled(bool value);

    bool running() const;

    bool allowLan() const;
    void setAllowLan(bool value);

    int port() const;
    void setPort(int value);

    QString apiKey() const;
    void setApiKey(const QString &value);

    QString bindAddress() const;
    QString baseUrl() const;
    QString lastError() const;

signals:
    void enabledChanged();
    void runningChanged();
    void allowLanChanged();
    void portChanged();
    void apiKeyChanged();
    void lastErrorChanged();

private slots:
    void syncFromSettings();
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();

private:
    struct HttpRequest {
        QString method;
        QString target;
        QString path;
        QHostAddress peerAddress;
        QHash<QString, QString> headers;
        QByteArray body;
        QJsonObject jsonBody;
    };

    struct HttpResponse {
        int status = 200;
        QByteArray body;
        QByteArray contentType = "application/json; charset=utf-8";
        QList<QPair<QByteArray, QByteArray>> headers;
    };

    struct MultipartPart {
        QHash<QString, QString> headers;
        QByteArray body;
    };

    struct CustomVoice {
        QString id;
        QString name;
        QString consentId;
        QString samplePath;
        QString fileName;
        QString mimeType;
        qint64 createdAt = 0;
    };

    void applySettingsState();
    bool startServer();
    void stopServer();
    void restartServer();
    void setLastError(const QString &value);
    void clearLastError();

    QJsonObject buildHealthDocument() const;
    QJsonObject buildModelsDocument() const;
    QJsonObject buildVoicesDocument() const;

    HttpResponse handleRequest(const HttpRequest &request);
    HttpResponse handleSpeechRequest(const HttpRequest &request);
    HttpResponse handleTranscriptionRequest(const HttpRequest &request);
    HttpResponse handleCreateVoiceConsentRequest(const HttpRequest &request);
    HttpResponse handleCreateVoiceRequest(const HttpRequest &request);
    HttpResponse handleVoiceDesignRequest(const HttpRequest &request);

    bool parseHttpRequest(QByteArray &buffer, HttpRequest &request, int &consumed);
    HttpResponse unauthorizedResponse() const;
    HttpResponse errorResponse(int status, const QString &message, const QString &type = QStringLiteral("invalid_request_error")) const;
    HttpResponse jsonResponse(const QJsonObject &object, int status = 200) const;
    HttpResponse binaryResponse(const QByteArray &data, const QByteArray &contentType, int status = 200) const;
    void writeResponse(QTcpSocket *socket, const HttpResponse &response);
    void processRequestAsync(const QPointer<QTcpSocket> &socket, const HttpRequest &request);

    static QJsonObject jsonErrorObject(const QString &message, const QString &type = QStringLiteral("invalid_request_error"));
    static QString guessContentType(const QString &format);
    static QString randomApiKey();
    static QString randomObjectId(const QString &prefix);

    bool checkAuthorization(const HttpRequest &request) const;
    HttpResponse runTtsGeneration(const QString &input,
                                  const QString &format,
                                  const QVariantMap &settings,
                                  float speed,
                                  const QString &mode,
                                  const QString &referencePath = QString(),
                                  const QString &modelId = QString());
    QVariantMap extraSettingsFromJson(const QJsonObject &json) const;
    QVariantMap extraSettingsFromMultipart(const QHash<QString, QString> &fields) const;
    QVariantMap currentVoiceSettings() const;
    QVariantList availableVoiceEntries() const;
    QVariantList availableModelEntries() const;
    QJsonObject parseJsonObject(const QByteArray &body, QString *error = nullptr) const;
    bool parseMultipart(const QByteArray &body,
                        const QString &contentType,
                        QHash<QString, QString> *fields,
                        QByteArray *fileData,
                        QString *fileName,
                        QString *mimeType,
                        QString *error) const;
    static QString multipartBoundary(const QString &contentType);
    static QByteArray trimMultipartPart(const QByteArray &part);
    static QHash<QString, QString> parseDispositionParams(const QString &value);
    static QHash<QString, QString> parseHeaderLines(const QByteArray &headersBlob);

    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    Settings *m_settings = nullptr;
    TtsEngine *m_tts = nullptr;
    SttEngine *m_stt = nullptr;
    bool m_enabled = false;
    bool m_running = false;
    bool m_allowLan = false;
    int m_port = 0;
    QString m_apiKey;
    QString m_lastError;
    QHash<QString, CustomVoice> m_customVoices;
    QHash<QString, QJsonObject> m_voiceConsents;
};

} // namespace LAStudio
