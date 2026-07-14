#include "Logger.h"
#include "PathUtils.h"
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QSysInfo>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LAStudio {

// Define modular logging categories
Q_LOGGING_CATEGORY(logCore, "lastudio.core")
Q_LOGGING_CATEGORY(logSTT, "lastudio.stt")
Q_LOGGING_CATEGORY(logTTS, "lastudio.tts")
Q_LOGGING_CATEGORY(logDownload, "lastudio.download")
Q_LOGGING_CATEGORY(logHardware, "lastudio.hardware")
Q_LOGGING_CATEGORY(logUI, "lastudio.ui")

static QFile *s_logFile = nullptr;
static QMutex s_logMutex;
static qint64 s_sessionStartOffset = 0;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Skip noisy DirectWrite warnings about legacy fonts on Windows
    if (msg.startsWith(QLatin1String("DirectWrite: CreateFontFaceFromHDC() failed"))) {
        return;
    }
    
    QMutexLocker locker(&s_logMutex);
    
    QString typeStr;
    switch (type) {
    case QtDebugMsg:    typeStr = "DBG"; break;
    case QtInfoMsg:     typeStr = "INF"; break;
    case QtWarningMsg:  typeStr = "WRN"; break;
    case QtCriticalMsg: typeStr = "ERR"; break;
    case QtFatalMsg:    typeStr = "FTL"; break;
    }

    // 1. Get Thread ID (hex formatted)
    QString threadIdStr = QString::asprintf("0x%zx", reinterpret_cast<size_t>(QThread::currentThreadId()));

    // 2. Extract Source Code Location (File + Line) in Debug mode
    QString locationStr;
#ifdef QT_DEBUG
    if (context.file && context.line > 0) {
        QString filePath = QString::fromUtf8(context.file);
        int lastSlash = filePath.lastIndexOf('/');
        if (lastSlash == -1) lastSlash = filePath.lastIndexOf('\\');
        QString fileName = (lastSlash != -1) ? filePath.mid(lastSlash + 1) : filePath;
        locationStr = QString(" [%1:%2]").arg(fileName).arg(context.line);
    }
#endif

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString catStr = context.category ? QString::fromUtf8(context.category) : QStringLiteral("default");
    QString finalMsg = msg;
    
    // Dynamically parse category prefixes like "[TtsWorker] Loaded" from legacy string logs
    if ((catStr == QStringLiteral("default") || catStr == QStringLiteral("qml")) && msg.startsWith('[')) {
        int closeBracket = msg.indexOf(']');
        if (closeBracket > 1) {
            QString extractedCat = msg.mid(1, closeBracket - 1).trimmed();
            catStr = QStringLiteral("lastudio.%1").arg(extractedCat.toLower());
            finalMsg = msg.mid(closeBracket + 1).trimmed();
        }
    }
    
    // Format: [YYYY-MM-DD HH:mm:ss.zzz] [LEVEL] [Thread 0xTID] [Category] [File:Line] Message
    const QString line = QStringLiteral("[%1] [%2] [T:%3] [%4]%5 %6\n")
                            .arg(ts)
                            .arg(typeStr)
                            .arg(threadIdStr)
                            .arg(catStr)
                            .arg(locationStr)
                            .arg(finalMsg);

    // Write UTF-8 to stderr to preserve non-ASCII logs (e.g., Vietnamese).
    const QByteArray utf8Line = line.toUtf8();
    fwrite(utf8Line.constData(), 1, static_cast<size_t>(utf8Line.size()), stderr);
    fflush(stderr);

    // Write to file with rotation check
    if (s_logFile && s_logFile->isOpen()) {
        s_logFile->write(line.toUtf8());
        s_logFile->flush();
        
        // Handle log rotation if current file exceeds limit
        if (s_logFile->size() >= Logger::MaxLogFileSize) {
            s_logFile->close();
            Logger::rotateLogs();
            
            // Reopen new app.log file
            s_logFile->setFileName(PathUtils::logsDir() + "/app.log");
            s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        }
    }
}

void Logger::rotateLogs()
{
    QString logDir = PathUtils::logsDir();
    QString mainLog = logDir + "/app.log";
    
    // Shift historical log files (app.4.log -> app.5.log, etc.)
    for (int i = MaxLogFiles - 1; i >= 1; --i) {
        QString oldName = QString("%1/app.%2.log").arg(logDir).arg(i);
        QString newName = QString("%1/app.%2.log").arg(logDir).arg(i + 1);
        if (QFile::exists(newName)) {
            QFile::remove(newName);
        }
        if (QFile::exists(oldName)) {
            QFile::rename(oldName, newName);
        }
    }
    
    // Shift current main log to app.1.log
    QString backupName = logDir + "/app.1.log";
    if (QFile::exists(backupName)) {
        QFile::remove(backupName);
    }
    QFile::rename(mainLog, backupName);
}

void Logger::init()
{
    QMutexLocker locker(&s_logMutex);
    if (s_logFile) return;

#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    PathUtils::ensureDirsExist();
    QString logPath = PathUtils::logsDir() + "/app.log";
    
    // If log file is already over-limit at startup, rotate it immediately
    QFile file(logPath);
    if (file.exists() && file.size() >= MaxLogFileSize) {
        rotateLogs();
    }

    QFileInfo logInfo(logPath);
    s_sessionStartOffset = logInfo.exists() ? logInfo.size() : 0;
    
    s_logFile = new QFile(logPath);
    // Opened in Append mode to preserve log files across app restarts
    if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qInstallMessageHandler(messageHandler);
    } else {
        delete s_logFile;
        s_logFile = nullptr;
    }
    
    // Write system/app information banner at startup
    locker.unlock(); // Unlock mutex before invoking QLoggingCategory to prevent recursive locks
    
    qCInfo(logCore) << "==================================================";
    qCInfo(logCore) << "LA Studio Starting...";
    qCInfo(logCore) << "App Version:   " << QCoreApplication::applicationVersion();
    qCInfo(logCore) << "OS Version:    " << QSysInfo::prettyProductName();
    qCInfo(logCore) << "Architecture:  " << QSysInfo::currentCpuArchitecture();
    qCInfo(logCore) << "==================================================";
}

qint64 Logger::sessionStartOffset()
{
    QMutexLocker locker(&s_logMutex);
    return s_sessionStartOffset;
}

void Logger::clear()
{
    QMutexLocker locker(&s_logMutex);
    if (s_logFile && s_logFile->isOpen()) {
        s_logFile->close();
        if (s_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            // Reopened and truncated successfully
        }
    }
}

void Logger::log(Level level, const QString &category, const QString &message)
{
    const QString line = QStringLiteral("[%1] %2").arg(category, message);

    switch (level) {
    case Debug:   qDebug().noquote() << line; break;
    case Info:    qInfo().noquote() << line; break;
    case Warning: qWarning().noquote() << line; break;
    case Error:   qCritical().noquote() << line; break;
    }
}

} // namespace LAStudio


