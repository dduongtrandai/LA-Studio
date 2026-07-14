#include "QmlLogger.h"
#include "Logger.h"

namespace LAStudio {

QmlLogger::QmlLogger(QObject *parent) : QObject(parent) {}

void QmlLogger::debug(const QString &category, const QString &message) {
    Logger::debug(category, message);
}

void QmlLogger::info(const QString &category, const QString &message) {
    Logger::info(category, message);
}

void QmlLogger::warning(const QString &category, const QString &message) {
    Logger::warning(category, message);
}

void QmlLogger::error(const QString &category, const QString &message) {
    Logger::error(category, message);
}

} // namespace LAStudio
