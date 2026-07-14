#include "LogViewService.h"

#include "core/Logger.h"
#include "core/PathUtils.h"

#include <QCoreApplication>
#include <QFile>
#include <QPointer>
#include <QRegularExpression>
#include <QTextStream>
#include <QThreadPool>

namespace LAStudio {
namespace {

QString readLogFileFromOffset(qint64 offset)
{
    const QString logPath = PathUtils::logsDir() + QStringLiteral("/app.log");
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Failed to open app.log");
    }

    if (file.size() < offset) {
        offset = 0;
    }
    if (offset > 0 && !file.seek(offset)) {
        return QString();
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    return in.readAll();
}

QString formatLogContent(const QString &rawContent)
{
    QStringList lines = rawContent.split(QLatin1Char('\n'));
    QStringList formattedLines;
    static const QRegularExpression regex(
        QStringLiteral(R"(^\[([\d\-:\.\s]+)\]\s+\[([A-Z]{3})\]\s+\[(T:0x[0-9a-fA-F]+)\]\s+\[([^\]]+)\](?:\s+\[([^\]]+)\])?\s+(.*)$)")
    );

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        const QString escaped = trimmed.toHtmlEscaped();
        const auto match = regex.match(escaped);
        if (match.hasMatch()) {
            const QString ts = match.captured(1);
            const QString level = match.captured(2);
            const QString thread = match.captured(3);
            const QString category = match.captured(4);
            const QString location = match.captured(5);
            const QString msg = match.captured(6);
            QString levelColor = QStringLiteral("#abb2bf");
            bool isBold = false;

            if (level == QLatin1String("DBG")) levelColor = QStringLiteral("#5c6370");
            else if (level == QLatin1String("INF")) levelColor = QStringLiteral("#98c379");
            else if (level == QLatin1String("WRN")) levelColor = QStringLiteral("#d19a66");
            else if (level == QLatin1String("ERR") || level == QLatin1String("FTL")) {
                levelColor = QStringLiteral("#e06c75");
                isBold = true;
            }

            QString formatted = QStringLiteral("<font color='#5c6370'>[%1]</font> "
                                               "<font color='%2'><b>[%3]</b></font> "
                                               "<font color='#c678dd'>[%4]</font> "
                                               "<font color='#61afef'>[%5]</font> ")
                                    .arg(ts, levelColor, level, thread, category);
            if (!location.isEmpty()) {
                formatted += QStringLiteral("<font color='#e5c07b'>[%1]</font> ").arg(location);
            }
            formatted += QStringLiteral("<font color='%1'>%2%3%4</font>")
                             .arg(isBold ? levelColor : QStringLiteral("#abb2bf"),
                                  isBold ? QStringLiteral("<b>") : QString(),
                                  msg,
                                  isBold ? QStringLiteral("</b>") : QString());
            formattedLines.append(formatted);
        } else {
            QString fallbackColor = QStringLiteral("#abb2bf");
            if (escaped.contains(QLatin1String("[DBG]"))) fallbackColor = QStringLiteral("#5c6370");
            else if (escaped.contains(QLatin1String("[WRN]"))) fallbackColor = QStringLiteral("#d19a66");
            else if (escaped.contains(QLatin1String("[ERR]"))) fallbackColor = QStringLiteral("#e06c75");
            else if (escaped.contains(QLatin1String("[FTL]"))) fallbackColor = QStringLiteral("#e06c75");
            formattedLines.append(QStringLiteral("<font color='%1'>%2</font>").arg(fallbackColor, escaped));
        }
    }

    return formattedLines.join(QStringLiteral("<br>"));
}

} // namespace

LogViewService::LogViewService(QObject *parent)
    : QObject(parent)
    , m_sessionStartPosition(Logger::sessionStartOffset())
    , m_lastLogPosition(m_sessionStartPosition)
{
}

void LogViewService::requestLoadLogs()
{
    if (m_loading) return;
    m_loading = true;
    emit loadingChanged();

    QPointer<LogViewService> weakThis(this);
    const qint64 sessionStartPosition = m_sessionStartPosition;
    QThreadPool::globalInstance()->start([weakThis, sessionStartPosition]() {
        const QString finalHtml = formatLogContent(readLogFileFromOffset(sessionStartPosition));

        QCoreApplication *app = QCoreApplication::instance();
        if (app) {
            QMetaObject::invokeMethod(app, [weakThis, finalHtml]() {
                if (!weakThis) return;
                weakThis->m_logContent = finalHtml;
                weakThis->m_loading = false;
                emit weakThis->logContentChanged();
                emit weakThis->loadingChanged();
            });
        }
    });
}

void LogViewService::clearLogFile()
{
    Logger::clear();
    resetLogPosition();
}

QString LogViewService::readNewLogs()
{
    const QString logPath = PathUtils::logsDir() + QStringLiteral("/app.log");
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    const qint64 fileSize = file.size();
    if (fileSize < m_lastLogPosition) {
        m_sessionStartPosition = 0;
        m_lastLogPosition = 0;
    }

    if (!file.seek(m_lastLogPosition)) {
        return QString();
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    const QString content = in.readAll();
    m_lastLogPosition = file.pos();

    return content.isEmpty() ? QString() : formatLogContent(content);
}

void LogViewService::resetLogPosition()
{
    m_sessionStartPosition = 0;
    m_lastLogPosition = 0;
}

QString LogViewService::readLogFile() const
{
    return readSessionLogFile();
}

QString LogViewService::readSessionLogFile() const
{
    return readLogFileFromOffset(m_sessionStartPosition);
}

} // namespace LAStudio
