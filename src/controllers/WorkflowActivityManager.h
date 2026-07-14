#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QtQml/qqml.h>

namespace LAStudio {

class IModelSession;
class AlignmentExecutionService;
class ModelSessionRegistry;
class SttSessionController;
class TtsEngine;
class DubbingController;

class WorkflowActivityManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("WorkflowActivityManager is managed by AppController")

    Q_PROPERTY(QVariantList activeWorkflows READ activeWorkflows NOTIFY workflowsChanged)
    Q_PROPERTY(QVariantList activeSessions READ activeSessions NOTIFY workflowsChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY workflowsChanged)
    Q_PROPERTY(int sessionCount READ sessionCount NOTIFY workflowsChanged)
    Q_PROPERTY(int runningCount READ runningCount NOTIFY workflowsChanged)
    Q_PROPERTY(bool hasActiveWorkflows READ hasActiveWorkflows NOTIFY workflowsChanged)

public:
    explicit WorkflowActivityManager(ModelSessionRegistry *sessionRegistry,
                             TtsEngine *tts,
                             SttSessionController *sttSession,
                             AlignmentExecutionService *alignment,
                             DubbingController *dubbing = nullptr,
                             QObject *parent = nullptr);

    QVariantList activeWorkflows() const;
    QVariantList activeSessions() const;
    int activeCount() const;
    int sessionCount() const;
    int runningCount() const;
    bool hasActiveWorkflows() const { return activeCount() > 0 || sessionCount() > 0; }

    Q_INVOKABLE void stopWorkflow(const QString &id);
    Q_INVOKABLE void openWorkflow(const QString &id);

signals:
    void workflowsChanged();
    void openRequested(const QString &routeId);

private slots:
    void refresh();

private:
    QVariantMap ttsWorkflow() const;
    QVariantMap sttWorkflow() const;
    QVariantMap alignmentWorkflow() const;
    QVariantMap dubbingWorkflow() const;
    QVariantList sessionItems(IModelSession *session) const;
    QVariantMap makeWorkflow(const QString &id,
                             const QString &type,
                             const QString &title,
                             const QString &routeId,
                             const QString &iconName,
                             int progress,
                             bool progressEstimated,
                             const QString &stageLabel,
                             bool cancellable) const;
    void updateActiveStartTimes(const QVariantList &workflows) const;
    static QString stateLabel(int stateValue);
    static QString iconForCapability(const QString &capabilityId);
    static QString fallbackTitleForCapability(const QString &capabilityId);
    static QString routeForCapability(const QString &capabilityId);
    static QString studioRouteForCapability(const QString &capabilityId);
    static QString statusLabel(bool stopping);
    static QString ttsTitleForMode(const QString &mode);

    ModelSessionRegistry *m_sessionRegistry = nullptr;
    TtsEngine *m_tts = nullptr;
    SttSessionController *m_sttSession = nullptr;
    AlignmentExecutionService *m_alignment = nullptr;
    DubbingController *m_dubbing = nullptr;
    mutable QHash<QString, QDateTime> m_startedAtById;
    mutable QSet<QString> m_stoppingIds;
};

} // namespace LAStudio
