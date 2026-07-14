#include "WorkflowRunJournal.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

namespace LAStudio {

namespace {
void setError(QString *error, const QString &message) { if (error) *error = message; }
QString safeId(const QString &value)
{
    QString result = value.trimmed();
    result.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
    return result.isEmpty() ? QStringLiteral("unknown") : result;
}
}

QJsonObject WorkflowRunEvent::toJson() const
{
    return QJsonObject{{QStringLiteral("sequence"), static_cast<qint64>(sequence)},
                       {QStringLiteral("timestamp"), timestamp.toString(Qt::ISODateWithMs)},
                       {QStringLiteral("event"), eventType},
                       {QStringLiteral("runId"), runId},
                       {QStringLiteral("nodeRunId"), nodeRunId},
                       {QStringLiteral("payload"), payload}};
}

WorkflowRunEvent WorkflowRunEvent::fromJson(const QJsonObject &json)
{
    WorkflowRunEvent result;
    result.sequence = json.value(QStringLiteral("sequence")).toVariant().toULongLong();
    result.timestamp = QDateTime::fromString(json.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    result.eventType = json.value(QStringLiteral("event")).toString();
    result.runId = json.value(QStringLiteral("runId")).toString();
    result.nodeRunId = json.value(QStringLiteral("nodeRunId")).toString();
    result.payload = json.value(QStringLiteral("payload")).toObject();
    return result;
}

WorkflowRunJournal::WorkflowRunJournal(QString rootPath)
    : m_rootPath(QDir::cleanPath(std::move(rootPath))) {}

QString WorkflowRunJournal::pathFor(const QString &runId) const
{
    return QDir(m_rootPath).filePath(QStringLiteral("runs/%1.jsonl").arg(safeId(runId)));
}

bool WorkflowRunJournal::append(WorkflowRunEvent event, QString *error) const
{
    if (event.runId.isEmpty() || event.eventType.isEmpty()) {
        setError(error, QStringLiteral("Workflow event requires runId and eventType."));
        return false;
    }
    const QString path = pathFor(event.runId);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setError(error, QStringLiteral("Cannot create workflow journal directory."));
        return false;
    }
    const QList<WorkflowRunEvent> existing = read(event.runId, nullptr);
    event.sequence = existing.isEmpty() ? 1 : existing.constLast().sequence + 1;
    if (!event.timestamp.isValid()) event.timestamp = QDateTime::currentDateTimeUtc();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        setError(error, file.errorString());
        return false;
    }
    const QByteArray line = QJsonDocument(event.toJson()).toJson(QJsonDocument::Compact) + '\n';
    if (file.write(line) != line.size()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

QList<WorkflowRunEvent> WorkflowRunJournal::read(const QString &runId, QString *error) const
{
    QList<WorkflowRunEvent> result;
    QFile file(pathFor(runId));
    if (!file.exists()) return result;
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return result;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            setError(error, parseError.errorString());
            result.clear();
            return result;
        }
        result.append(WorkflowRunEvent::fromJson(document.object()));
    }
    return result;
}

} // namespace LAStudio
