#pragma once

#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QJsonObject>
#include <atomic>

namespace LAStudio {

enum class WorkflowRunState { Queued, Running, WaitingForInput, Completed, Failed, Cancelled, Interrupted };

QString workflowRunStateName(WorkflowRunState state);

class WorkflowCancellationToken final
{
public:
    void requestCancel() noexcept { m_cancelled.store(true, std::memory_order_release); }
    bool isCancelled() const noexcept { return m_cancelled.load(std::memory_order_acquire); }

private:
    std::atomic_bool m_cancelled = false;
};

struct WorkflowRunIdentity
{
    QString runId;
    QString nodeRunId;
    QString workflowId;
    int workflowVersion = 1;
    QString nodeId;
    QString nodeType;
    int nodeContractVersion = 1;
    int attempt = 1;
};

struct WorkflowReviewRequest
{
    QString reviewId;
    QString runId;
    QString nodeRunId;
    QString nodeId;
    QString mode;
    QString editor;
    QVariant artifact;
    QDateTime createdAt;

    QJsonObject toJson() const;
    static WorkflowReviewRequest fromJson(const QJsonObject &json);
    bool isValid() const { return !reviewId.isEmpty() && !runId.isEmpty() && !nodeId.isEmpty(); }
};

class WorkflowReviewStore final
{
public:
    explicit WorkflowReviewStore(QString rootPath);
    bool save(const WorkflowReviewRequest &request, QString *error = nullptr) const;
    bool load(const QString &reviewId, WorkflowReviewRequest &request, QString *error = nullptr) const;
    bool remove(const QString &reviewId, QString *error = nullptr) const;

private:
    QString pathFor(const QString &reviewId) const;
    QString m_rootPath;
};

} // namespace LAStudio
