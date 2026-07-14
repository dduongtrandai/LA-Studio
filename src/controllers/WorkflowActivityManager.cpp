#include "WorkflowActivityManager.h"

#include "IModelSession.h"
#include "ModelSessionRegistry.h"
#include "AlignmentExecutionService.h"
#include "DubbingController.h"
#include "SttSessionController.h"
#include "tts/TtsEngine.h"

#include <QFileInfo>
#include <QTimer>

namespace LAStudio {

WorkflowActivityManager::WorkflowActivityManager(ModelSessionRegistry *sessionRegistry,
                                             TtsEngine *tts,
                                             SttSessionController *sttSession,
                                             AlignmentExecutionService *alignment,
                                             DubbingController *dubbing,
                                             QObject *parent)
    : QObject(parent)
    , m_sessionRegistry(sessionRegistry)
    , m_tts(tts)
    , m_sttSession(sttSession)
    , m_alignment(alignment)
    , m_dubbing(dubbing)
{
    if (m_sessionRegistry) {
        for (IModelSession *session : m_sessionRegistry->sessions()) {
            if (!session) {
                continue;
            }
            connect(session, &IModelSession::stateChanged, this, &WorkflowActivityManager::refresh);
            connect(session, &IModelSession::activeConfigurationChanged, this, &WorkflowActivityManager::refresh);
            connect(session, &IModelSession::activeSignatureChanged, this, &WorkflowActivityManager::refresh);
        }
    }

    if (m_tts) {
        connect(m_tts, &TtsEngine::processingChanged, this, &WorkflowActivityManager::refresh);
        connect(m_tts, &TtsEngine::stateChanged, this, &WorkflowActivityManager::refresh);
        connect(m_tts, &TtsEngine::generationProgressChanged, this, &WorkflowActivityManager::refresh);
        connect(m_tts, &TtsEngine::lastGenerationModeChanged, this, &WorkflowActivityManager::refresh);
        connect(m_tts, &TtsEngine::cpuUsageChanged, this, &WorkflowActivityManager::refresh);
        connect(m_tts, &TtsEngine::memoryUsageChanged, this, &WorkflowActivityManager::refresh);
    }

    if (m_sttSession) {
        connect(m_sttSession, &SttSessionController::processingChanged, this, &WorkflowActivityManager::refresh);
        connect(m_sttSession, &SttSessionController::progressChanged, this, &WorkflowActivityManager::refresh);
    }

    if (m_alignment) {
        connect(m_alignment, &AlignmentExecutionService::stateChanged, this, &WorkflowActivityManager::refresh);
        connect(m_alignment, &AlignmentExecutionService::completed, this, &WorkflowActivityManager::refresh);
        connect(m_alignment, &AlignmentExecutionService::failed, this, &WorkflowActivityManager::refresh);
    }
    if (m_dubbing) {
        connect(m_dubbing, &DubbingController::processingChanged, this, &WorkflowActivityManager::refresh);
    }

    auto *elapsedTimer = new QTimer(this);
    elapsedTimer->setInterval(1000);
    connect(elapsedTimer, &QTimer::timeout, this, [this]() {
        if (hasActiveWorkflows()) {
            emit workflowsChanged();
        }
    });
    elapsedTimer->start();
}

QVariantList WorkflowActivityManager::activeWorkflows() const
{
    QVariantList workflows;

    const QVariantMap tts = ttsWorkflow();
    if (!tts.isEmpty()) {
        workflows.append(tts);
    }

    const QVariantMap stt = sttWorkflow();
    if (!stt.isEmpty()) {
        workflows.append(stt);
    }

    const QVariantMap alignment = alignmentWorkflow();
    if (!alignment.isEmpty()) {
        workflows.append(alignment);
    }

    const QVariantMap dubbing = dubbingWorkflow();
    if (!dubbing.isEmpty()) {
        workflows.append(dubbing);
    }

    updateActiveStartTimes(workflows);
    return workflows;
}

QVariantList WorkflowActivityManager::activeSessions() const
{
    QVariantList sessions;
    if (!m_sessionRegistry) {
        return sessions;
    }

    for (IModelSession *session : m_sessionRegistry->sessions()) {
        const QVariantList items = sessionItems(session);
        for (const QVariant &item : items) {
            sessions.append(item);
        }
    }

    return sessions;
}

int WorkflowActivityManager::activeCount() const
{
    return runningCount();
}

int WorkflowActivityManager::sessionCount() const
{
    return activeSessions().size();
}

int WorkflowActivityManager::runningCount() const
{
    return activeWorkflows().size();
}

void WorkflowActivityManager::stopWorkflow(const QString &id)
{
    if (id == QStringLiteral("tts-active")) {
        if (m_tts && m_tts->isProcessing()) {
            m_stoppingIds.insert(id);
            m_tts->cancelProcessing();
            refresh();
        }
        return;
    }

    if (id == QStringLiteral("stt-active")) {
        if (m_sttSession && m_sttSession->processing()) {
            m_stoppingIds.insert(id);
            m_sttSession->cancelProcessing();
            refresh();
        }
        return;
    }

    if (id == QStringLiteral("alignment-active")) {
        if (m_alignment && m_alignment->processing()) {
            m_stoppingIds.insert(id);
            m_alignment->cancel();
            refresh();
        }
    }

    if (id == QStringLiteral("dubbing-active")) {
        if (m_dubbing && m_dubbing->processing()) {
            m_stoppingIds.insert(id);
            m_dubbing->cancelProcessing();
            refresh();
        }
    }
}

void WorkflowActivityManager::openWorkflow(const QString &id)
{
    if (id == QStringLiteral("stt-active")) {
        emit openRequested(studioRouteForCapability(QStringLiteral("stt")));
        return;
    }

    if (id == QStringLiteral("alignment-active")) {
        emit openRequested(studioRouteForCapability(QStringLiteral("forced-alignment")));
        return;
    }

    if (id == QStringLiteral("dubbing-active")) {
        emit openRequested(QStringLiteral("studio-dubbing"));
        return;
    }

    if (id.startsWith(QStringLiteral("session:"))) {
        const QString payload = id.mid(QStringLiteral("session:").size());
        const QString capabilityId = payload.section(QLatin1Char(':'), 0, 0);
        emit openRequested(studioRouteForCapability(capabilityId));
        return;
    }

    if (id == QStringLiteral("tts-active") && m_tts) {
        const QString mode = m_tts->lastGenerationMode();
        if (mode == QStringLiteral("voice-cloning")) {
            emit openRequested(studioRouteForCapability(QStringLiteral("voice-cloning")));
        } else if (mode == QStringLiteral("voice-design")) {
            emit openRequested(studioRouteForCapability(QStringLiteral("voice-design")));
        } else {
            emit openRequested(studioRouteForCapability(QStringLiteral("tts")));
        }
    }
}

void WorkflowActivityManager::refresh()
{
    emit workflowsChanged();
}

QVariantMap WorkflowActivityManager::ttsWorkflow() const
{
    if (!m_tts || !m_tts->isProcessing()) {
        return {};
    }

    const QString mode = m_tts->lastGenerationMode();
    QString routeId = studioRouteForCapability(QStringLiteral("tts"));
    QString iconName = QStringLiteral("volume");
    if (mode == QStringLiteral("voice-cloning")) {
        routeId = studioRouteForCapability(QStringLiteral("voice-cloning"));
        iconName = QStringLiteral("mic");
    } else if (mode == QStringLiteral("voice-design")) {
        routeId = studioRouteForCapability(QStringLiteral("voice-design"));
        iconName = QStringLiteral("spark");
    }

    return makeWorkflow(QStringLiteral("tts-active"),
                        mode.isEmpty() ? QStringLiteral("tts") : mode,
                        ttsTitleForMode(mode),
                        routeId,
                        iconName,
                        m_tts->generationProgress(),
                        m_tts->generationProgressEstimated(),
                        m_tts->generationProgressLabel(),
                        true);
}

QVariantList WorkflowActivityManager::sessionItems(IModelSession *session) const
{
    QVariantList items;
    if (!session) {
        return items;
    }

    const QString activeSignature = session->activeSignature();
    for (const SessionConfiguration &config : session->loadedConfigurations()) {
        const QString capabilityId = routeForCapability(config.capabilityId);
        const QString title = config.familyConfig
            .value(QStringLiteral("title"), fallbackTitleForCapability(capabilityId))
            .toString();
        const QString runtimeId = config.selection.runtimeId;
        const QString runtimeVersion = config.selection.runtimeVersion;
        const QString runtimeLabel = runtimeVersion.isEmpty()
            ? runtimeId
            : QStringLiteral("%1 %2").arg(runtimeId, runtimeVersion);

        QVariantList files;
        for (const QString &path : config.resolvedModelPaths) {
            QVariantMap file;
            file.insert(QStringLiteral("name"), QFileInfo(path).fileName());
            file.insert(QStringLiteral("path"), path);
            files.append(file);
        }

        const bool isTtsShared = capabilityId == QStringLiteral("tts") ||
                                 capabilityId == QStringLiteral("voice-cloning") ||
                                 capabilityId == QStringLiteral("voice-design");
        const bool isDefault = config.signature == activeSignature;
        const QString status = isDefault ? stateLabel(static_cast<int>(session->state())) : QStringLiteral("Ready");

        QVariantMap item;
        item.insert(QStringLiteral("id"), QStringLiteral("session:%1:%2").arg(capabilityId, config.signature));
        item.insert(QStringLiteral("capabilityId"), capabilityId);
        item.insert(QStringLiteral("title"), title);
        item.insert(QStringLiteral("routeId"), studioRouteForCapability(capabilityId));
        item.insert(QStringLiteral("iconName"), iconForCapability(capabilityId));
        item.insert(QStringLiteral("status"), status);
        item.insert(QStringLiteral("statusLabel"), isDefault ? QStringLiteral("Default") : QStringLiteral("Loaded"));
        item.insert(QStringLiteral("runtime"), runtimeLabel);
        item.insert(QStringLiteral("modelFileCount"), config.resolvedModelPaths.size());
        item.insert(QStringLiteral("modelFiles"), files);
        item.insert(QStringLiteral("active"), isDefault);
        item.insert(QStringLiteral("loaded"), true);
        item.insert(QStringLiteral("cpuUsage"), isDefault && isTtsShared && m_tts ? m_tts->cpuUsage() : 0.0);
        item.insert(QStringLiteral("ramUsage"), isDefault && isTtsShared && m_tts ? m_tts->estimatedRamUsage() : QString());
        item.insert(QStringLiteral("vramUsage"), isDefault && isTtsShared && m_tts ? m_tts->estimatedVramUsage() : QString());
        item.insert(QStringLiteral("hardwareRealtime"), isDefault && isTtsShared);
        items.append(item);
    }

    return items;
}

QVariantMap WorkflowActivityManager::sttWorkflow() const
{
    if (!m_sttSession || !m_sttSession->processing()) {
        return {};
    }

    return makeWorkflow(QStringLiteral("stt-active"),
                        QStringLiteral("stt"),
                        QStringLiteral("Transcribing audio"),
                        studioRouteForCapability(QStringLiteral("stt")),
                        QStringLiteral("waves"),
                        m_sttSession->progress(),
                        false,
                        QStringLiteral("Speech to text"),
                        true);
}

QVariantMap WorkflowActivityManager::alignmentWorkflow() const
{
    if (!m_alignment || !m_alignment->processing()) {
        return {};
    }

    return makeWorkflow(QStringLiteral("alignment-active"),
                        QStringLiteral("forced-alignment"),
                        QStringLiteral("Aligning transcript"),
                        studioRouteForCapability(QStringLiteral("forced-alignment")),
                        QStringLiteral("sliders"),
                        0,
                        true,
                        m_alignment->statusText().isEmpty() ? QStringLiteral("Alignment") : m_alignment->statusText(),
                        true);
}

QVariantMap WorkflowActivityManager::dubbingWorkflow() const
{
    if (!m_dubbing || !m_dubbing->processing()) return {};
    QVariantMap workflow = makeWorkflow(QStringLiteral("dubbing-active"),
                        QStringLiteral("dubbing"),
                        QStringLiteral("Running dubbing workflow"),
                        QStringLiteral("studio-dubbing"),
                        QStringLiteral("waves"),
                        m_dubbing->progress(),
                        false,
                        m_dubbing->stage().isEmpty() ? QStringLiteral("Dubbing") : m_dubbing->stage(),
                        true);
    workflow.insert(QStringLiteral("workflowId"), m_dubbing->workflowId());
    workflow.insert(QStringLiteral("workflowVersion"), m_dubbing->workflowVersion());
    workflow.insert(QStringLiteral("runId"), m_dubbing->workflowRunId());
    workflow.insert(QStringLiteral("nodeRunId"), m_dubbing->workflowNodeRunId());
    return workflow;
}

QVariantMap WorkflowActivityManager::makeWorkflow(const QString &id,
                                          const QString &type,
                                          const QString &title,
                                          const QString &routeId,
                                          const QString &iconName,
                                          int progress,
                                          bool progressEstimated,
                                          const QString &stageLabel,
                                          bool cancellable) const
{
    if (!m_startedAtById.contains(id)) {
        m_startedAtById.insert(id, QDateTime::currentDateTimeUtc());
    }

    const QDateTime startedAt = m_startedAtById.value(id);
    QVariantMap workflow;
    workflow.insert(QStringLiteral("id"), id);
    workflow.insert(QStringLiteral("type"), type);
    workflow.insert(QStringLiteral("title"), title);
    workflow.insert(QStringLiteral("routeId"), routeId);
    workflow.insert(QStringLiteral("iconName"), iconName);
    const bool stopping = m_stoppingIds.contains(id);
    workflow.insert(QStringLiteral("status"), stopping ? QStringLiteral("cancelling") : QStringLiteral("running"));
    workflow.insert(QStringLiteral("statusLabel"), statusLabel(stopping));
    workflow.insert(QStringLiteral("progress"), qBound(0, progress, 100));
    workflow.insert(QStringLiteral("progressEstimated"), progressEstimated);
    workflow.insert(QStringLiteral("stageLabel"), stageLabel);
    workflow.insert(QStringLiteral("cancellable"), cancellable);
    workflow.insert(QStringLiteral("pausable"), false);
    workflow.insert(QStringLiteral("startedAt"), startedAt.toString(Qt::ISODate));
    workflow.insert(QStringLiteral("elapsedSeconds"), startedAt.secsTo(QDateTime::currentDateTimeUtc()));
    return workflow;
}

void WorkflowActivityManager::updateActiveStartTimes(const QVariantList &workflows) const
{
    QHash<QString, bool> activeIds;
    for (const QVariant &entry : workflows) {
        const QString id = entry.toMap().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            activeIds.insert(id, true);
        }
    }

    for (auto it = m_startedAtById.begin(); it != m_startedAtById.end();) {
        if (!activeIds.contains(it.key())) {
            m_stoppingIds.remove(it.key());
            it = m_startedAtById.erase(it);
        } else {
            ++it;
        }
    }
}

QString WorkflowActivityManager::stateLabel(int stateValue)
{
    switch (static_cast<ModelSessionState>(stateValue)) {
    case ModelSessionState::Unconfigured:
        return QStringLiteral("Unconfigured");
    case ModelSessionState::Unloaded:
        return QStringLiteral("Unloaded");
    case ModelSessionState::Loading:
        return QStringLiteral("Loading");
    case ModelSessionState::Ready:
        return QStringLiteral("Ready");
    case ModelSessionState::Processing:
        return QStringLiteral("Processing");
    case ModelSessionState::Unloading:
        return QStringLiteral("Unloading");
    case ModelSessionState::Error:
        return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

QString WorkflowActivityManager::iconForCapability(const QString &capabilityId)
{
    if (capabilityId == QStringLiteral("stt")) {
        return QStringLiteral("waves");
    }
    if (capabilityId == QStringLiteral("voice-cloning")) {
        return QStringLiteral("mic");
    }
    if (capabilityId == QStringLiteral("voice-design")) {
        return QStringLiteral("spark");
    }
    if (capabilityId == QStringLiteral("forced-alignment")) {
        return QStringLiteral("sliders");
    }
    return QStringLiteral("volume");
}

QString WorkflowActivityManager::fallbackTitleForCapability(const QString &capabilityId)
{
    if (capabilityId == QStringLiteral("stt")) {
        return QStringLiteral("Speech to Text");
    }
    if (capabilityId == QStringLiteral("voice-cloning")) {
        return QStringLiteral("Voice Cloning");
    }
    if (capabilityId == QStringLiteral("voice-design")) {
        return QStringLiteral("Voice Design");
    }
    if (capabilityId == QStringLiteral("forced-alignment")) {
        return QStringLiteral("Alignment");
    }
    return QStringLiteral("Text to Speech");
}

QString WorkflowActivityManager::routeForCapability(const QString &capabilityId)
{
    if (capabilityId == QStringLiteral("voice-clone")) {
        return QStringLiteral("voice-cloning");
    }
    if (capabilityId.isEmpty()) {
        return QStringLiteral("tts");
    }
    return capabilityId;
}

QString WorkflowActivityManager::studioRouteForCapability(const QString &capabilityId)
{
    const QString normalized = routeForCapability(capabilityId);
    if (normalized == QStringLiteral("stt")) {
        return QStringLiteral("studio-stt");
    }
    if (normalized == QStringLiteral("voice-cloning")) {
        return QStringLiteral("studio-voice-cloning");
    }
    if (normalized == QStringLiteral("voice-design")) {
        return QStringLiteral("studio-voice-design");
    }
    if (normalized == QStringLiteral("forced-alignment")) {
        return QStringLiteral("studio-alignment");
    }
    return QStringLiteral("studio-tts");
}

QString WorkflowActivityManager::statusLabel(bool stopping)
{
    return stopping ? QStringLiteral("Stopping") : QStringLiteral("Running");
}

QString WorkflowActivityManager::ttsTitleForMode(const QString &mode)
{
    if (mode == QStringLiteral("voice-cloning")) {
        return QStringLiteral("Cloning voice");
    }
    if (mode == QStringLiteral("voice-design")) {
        return QStringLiteral("Designing voice");
    }
    return QStringLiteral("Generating speech");
}

} // namespace LAStudio
