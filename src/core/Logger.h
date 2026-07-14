#pragma once

#include <QString>
#include <QDebug>
#include <QDateTime>
#include <QLoggingCategory>
#include <QtGlobal>

namespace LAStudio {

// Declare logging categories for modular log filtering
Q_DECLARE_LOGGING_CATEGORY(logCore)
Q_DECLARE_LOGGING_CATEGORY(logSTT)
Q_DECLARE_LOGGING_CATEGORY(logTTS)
Q_DECLARE_LOGGING_CATEGORY(logDownload)
Q_DECLARE_LOGGING_CATEGORY(logHardware)
Q_DECLARE_LOGGING_CATEGORY(logUI)

class Logger {
public:
    enum Level { Debug, Info, Warning, Error };

    static void init();
    static void clear();
    static qint64 sessionStartOffset();
    static void log(Level level, const QString &category, const QString &message);
    static void debug(const QString &cat, const QString &msg)   { log(Debug, cat, msg); }
    static void info(const QString &cat, const QString &msg)    { log(Info, cat, msg); }
    static void warning(const QString &cat, const QString &msg) { log(Warning, cat, msg); }
    static void error(const QString &cat, const QString &msg)   { log(Error, cat, msg); }

    // Declared as friend to access private log rotation helpers
    friend void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    static void rotateLogs();
    
    static constexpr qint64 MaxLogFileSize = 5 * 1024 * 1024; // 5 MB limit per log file
    static constexpr int    MaxLogFiles    = 5;               // Keep up to 5 historical log files
};

} // namespace LAStudio


