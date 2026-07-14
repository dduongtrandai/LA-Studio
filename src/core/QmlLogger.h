#pragma once

#include <QObject>
#include <QString>

#include <QtQml/qqml.h>

namespace LAStudio {

class QmlLogger : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppLogger)
    QML_SINGLETON
public:
    explicit QmlLogger(QObject *parent = nullptr);

    Q_INVOKABLE void debug(const QString &category, const QString &message);
    Q_INVOKABLE void info(const QString &category, const QString &message);
    Q_INVOKABLE void warning(const QString &category, const QString &message);
    Q_INVOKABLE void error(const QString &category, const QString &message);
};

} // namespace LAStudio
