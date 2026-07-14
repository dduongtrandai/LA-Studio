#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

namespace LAStudio {

class FileAccessService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("FileAccessService is managed by AppController")

public:
    explicit FileAccessService(QObject *parent = nullptr);
    ~FileAccessService() override = default;

    Q_INVOKABLE QString urlToLocalPath(const QString &url) const;
    Q_INVOKABLE QString readTextFile(const QString &path) const;
    Q_INVOKABLE bool writeTextFile(const QString &path, const QString &content) const;
    Q_INVOKABLE bool localFileExists(const QString &path) const;
};

} // namespace LAStudio
