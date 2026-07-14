#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace LAStudio {

struct WorkflowRunEvent
{
    quint64 sequence = 0;
    QDateTime timestamp;
    QString eventType;
    QString runId;
    QString nodeRunId;
    QJsonObject payload;

    QJsonObject toJson() const;
    static WorkflowRunEvent fromJson(const QJsonObject &json);
};

class WorkflowRunJournal final
{
public:
    explicit WorkflowRunJournal(QString rootPath);

    bool append(WorkflowRunEvent event, QString *error = nullptr) const;
    QList<WorkflowRunEvent> read(const QString &runId, QString *error = nullptr) const;
    QString rootPath() const { return m_rootPath; }

private:
    QString pathFor(const QString &runId) const;
    QString m_rootPath;
};

} // namespace LAStudio
