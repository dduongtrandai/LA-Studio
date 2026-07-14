#include "WorkflowRun.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
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

QString workflowRunStateName(WorkflowRunState state)
{
    switch (state) {
    case WorkflowRunState::Queued: return QStringLiteral("queued");
    case WorkflowRunState::Running: return QStringLiteral("running");
    case WorkflowRunState::WaitingForInput: return QStringLiteral("waiting_for_input");
    case WorkflowRunState::Completed: return QStringLiteral("completed");
    case WorkflowRunState::Failed: return QStringLiteral("failed");
    case WorkflowRunState::Cancelled: return QStringLiteral("cancelled");
    case WorkflowRunState::Interrupted: return QStringLiteral("interrupted");
    }
    return QStringLiteral("failed");
}

QJsonObject WorkflowReviewRequest::toJson() const
{
    return QJsonObject{{QStringLiteral("reviewId"), reviewId},
                       {QStringLiteral("runId"), runId},
                       {QStringLiteral("nodeRunId"), nodeRunId},
                       {QStringLiteral("nodeId"), nodeId},
                       {QStringLiteral("mode"), mode},
                       {QStringLiteral("editor"), editor},
                       {QStringLiteral("artifact"), QJsonValue::fromVariant(artifact)},
                       {QStringLiteral("createdAt"), createdAt.toString(Qt::ISODateWithMs)}};
}

WorkflowReviewRequest WorkflowReviewRequest::fromJson(const QJsonObject &json)
{
    WorkflowReviewRequest result;
    result.reviewId = json.value(QStringLiteral("reviewId")).toString();
    result.runId = json.value(QStringLiteral("runId")).toString();
    result.nodeRunId = json.value(QStringLiteral("nodeRunId")).toString();
    result.nodeId = json.value(QStringLiteral("nodeId")).toString();
    result.mode = json.value(QStringLiteral("mode")).toString();
    result.editor = json.value(QStringLiteral("editor")).toString();
    result.artifact = json.value(QStringLiteral("artifact")).toVariant();
    result.createdAt = QDateTime::fromString(json.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    return result;
}

WorkflowReviewStore::WorkflowReviewStore(QString rootPath)
    : m_rootPath(QDir::cleanPath(std::move(rootPath))) {}

QString WorkflowReviewStore::pathFor(const QString &reviewId) const
{
    return QDir(m_rootPath).filePath(QStringLiteral("reviews/%1.json").arg(safeId(reviewId)));
}

bool WorkflowReviewStore::save(const WorkflowReviewRequest &request, QString *error) const
{
    if (!request.isValid()) {
        setError(error, QStringLiteral("Invalid workflow review request."));
        return false;
    }
    const QString path = pathFor(request.reviewId);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setError(error, QStringLiteral("Cannot create workflow review directory."));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, file.errorString());
        return false;
    }
    file.write(QJsonDocument(request.toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

bool WorkflowReviewStore::load(const QString &reviewId, WorkflowReviewRequest &request, QString *error) const
{
    QFile file(pathFor(reviewId));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, parseError.errorString());
        return false;
    }
    request = WorkflowReviewRequest::fromJson(document.object());
    if (!request.isValid()) {
        setError(error, QStringLiteral("Invalid stored workflow review request."));
        return false;
    }
    return true;
}

bool WorkflowReviewStore::remove(const QString &reviewId, QString *error) const
{
    const QString path = pathFor(reviewId);
    if (!QFileInfo::exists(path)) return true;
    if (!QFile::remove(path)) {
        setError(error, QStringLiteral("Cannot remove workflow review request: %1").arg(path));
        return false;
    }
    return true;
}

} // namespace LAStudio
