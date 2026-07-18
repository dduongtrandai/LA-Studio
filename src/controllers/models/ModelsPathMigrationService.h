#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

namespace LAStudio {

class Settings;
class ModelManager;
class DownloadManager;
class SttEngine;
class TtsEngine;

class ModelsPathMigrationService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ModelsPathMigrationService is managed by AppController")

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit ModelsPathMigrationService(Settings *settings,
                                        ModelManager *models,
                                        DownloadManager *downloads,
                                        SttEngine *stt,
                                        TtsEngine *tts,
                                        QObject *parent = nullptr);
    ~ModelsPathMigrationService() override = default;

    bool running() const { return m_running; }
    QString message() const { return m_message; }
    QString error() const { return m_error; }

    Q_INVOKABLE void changeDirectory(const QString &pathUrl);

signals:
    void runningChanged();
    void messageChanged();
    void errorChanged();

private:
    Settings *m_settings = nullptr;
    ModelManager *m_models = nullptr;
    DownloadManager *m_downloads = nullptr;
    SttEngine *m_stt = nullptr;
    TtsEngine *m_tts = nullptr;

    bool m_running = false;
    QString m_message;
    QString m_error;
};

} // namespace LAStudio
