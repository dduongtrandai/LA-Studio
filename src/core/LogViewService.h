#pragma once
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <QtQml/qqml.h>

namespace LAStudio {

class LogViewService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LogViewService is managed by AppController")
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString logContent READ logContent NOTIFY logContentChanged)
public:
    explicit LogViewService(QObject *parent = nullptr);
    ~LogViewService() override = default;

    bool isLoading() const { return m_loading; }
    QString logContent() const { return m_logContent; }

    Q_INVOKABLE void requestLoadLogs();
    Q_INVOKABLE void clearLogFile();
    Q_INVOKABLE QString readNewLogs();
    Q_INVOKABLE void resetLogPosition();
    Q_INVOKABLE QString readLogFile() const;

signals:
    void loadingChanged();
    void logContentChanged();

private:
    QString readSessionLogFile() const;

    bool m_loading = false;
    QString m_logContent;
    qint64 m_sessionStartPosition = 0;
    qint64 m_lastLogPosition = 0;
};

} // namespace LAStudio
