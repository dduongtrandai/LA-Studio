#include "controllers/dubbing/DubbingController.h"

#include "SttSessionController.h"
#include "tts/TtsEngine.h"
#include "controllers/dubbing/DubbingJobRunner.h"
#include "controllers/dubbing/DubbingTranslationFixService.h"
#include "dubbing/EspeakNgPhonemizer.h"
#include "dubbing/workflow/DubbingWorkflowDefinition.h"
#include "dubbing/workflow/DubbingWorkflowNodes.h"
#include "workflows/WorkflowGraphRunner.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include "controllers/app/AppController.h"
#include "controllers/models/ModelSessionRegistry.h"
#include "controllers/models/StudioConfigurationResolver.h"
#include "controllers/models/CapabilitySettingsSchema.h"
#include "controllers/models/DownloadInstallService.h"
#include "core/CapabilityFamilyModel.h"
#include "core/DownloadManager.h"
#include "core/ModelManager.h"
#include "core/RuntimeManager.h"

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtMath>
#include <QUrl>
#include <QUuid>
#include <QDir>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>

namespace LAStudio {

namespace {

QString automaticDefaultFamilyId(const QString &capabilityId,
                                 const QString &dubbingQuality = QString())
{
    if (capabilityId == QStringLiteral("stt"))
        return QStringLiteral("whisper.cpp");
    if (capabilityId == QStringLiteral("voice-isolation"))
        return QStringLiteral("sherpa-onnx-spleeter-2stems-fp16");
    if (capabilityId == QStringLiteral("translation"))
        return QStringLiteral("hy-mt2-1.8b");
    if (capabilityId == QStringLiteral("llm-chat"))
        return QStringLiteral("qwen3.5-2b");
    if (capabilityId == QStringLiteral("tts"))
        return dubbingQuality == QStringLiteral("fast")
            ? QStringLiteral("vieneu-tts-v2-turbo")
            : QStringLiteral("omnivoice");
    return {};
}

void unloadConflictingDubbingRuntime(ModelSessionRegistry *registry,
                                     const QString &capabilityId)
{
    if (!registry) return;

    QStringList conflictingCapabilities;
    if (capabilityId == QStringLiteral("tts") ||
        capabilityId == QStringLiteral("stt")) {
        conflictingCapabilities.append(QStringLiteral("translation"));
        conflictingCapabilities.append(QStringLiteral("llm-chat"));
    } else if (capabilityId == QStringLiteral("translation")) {
        conflictingCapabilities.append(QStringLiteral("stt"));
        conflictingCapabilities.append(QStringLiteral("tts"));
        conflictingCapabilities.append(QStringLiteral("llm-chat"));
    } else {
        return;
    }

    for (const QString &conflictingCapability : conflictingCapabilities) {
        IModelSession *conflictingSession =
            registry->sessionForCapability(conflictingCapability);
        if (!conflictingSession) continue;

        const QList<SessionConfiguration> loaded =
            conflictingSession->loadedConfigurations();
        if (loaded.isEmpty()) continue;

        Logger::info(
            QStringLiteral("DubbingController"),
            QStringLiteral("Dubbing runtime handoff: unloading %1 before loading %2 "
                           "to avoid incompatible shared DLLs in one process.")
                .arg(conflictingCapability, capabilityId));
        for (const SessionConfiguration &configuration : loaded) {
            conflictingSession->requestUnloadConfiguration(configuration.signature);
        }
    }
}

QString subtitleTimestamp(qint64 milliseconds, bool webVtt)
{
    const qint64 hours = milliseconds / 3600000;
    milliseconds %= 3600000;
    const qint64 minutes = milliseconds / 60000;
    milliseconds %= 60000;
    const qint64 seconds = milliseconds / 1000;
    const qint64 millis = milliseconds % 1000;
    return QStringLiteral("%1:%2:%3%4%5")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(webVtt ? QLatin1Char('.') : QLatin1Char(','))
        .arg(millis, 3, 10, QLatin1Char('0'));
}

bool writeDubbingSubtitles(const QVariantList &segments, const QString &path,
                           bool useTargetText, QString *error)
{
    const bool webVtt = QFileInfo(path).suffix().compare(QStringLiteral("vtt"), Qt::CaseInsensitive) == 0;
    QStringList lines;
    if (webVtt) lines.append(QStringLiteral("WEBVTT\n"));

    int cueNumber = 1;
    for (const QVariant &entry : segments) {
        const QVariantMap segment = entry.toMap();
        const QString text = segment.value(useTargetText ? QStringLiteral("targetText")
                                                         : QStringLiteral("sourceText")).toString().trimmed();
        if (text.isEmpty()) continue;
        if (!webVtt) lines.append(QString::number(cueNumber));
        lines.append(subtitleTimestamp(segment.value(QStringLiteral("startMs")).toLongLong(), webVtt)
                     + QStringLiteral(" --> ")
                     + subtitleTimestamp(segment.value(QStringLiteral("endMs")).toLongLong(), webVtt));
        lines.append(text);
        lines.append(QString());
        ++cueNumber;
    }
    if (cueNumber == 1) {
        if (error) *error = useTargetText
            ? QStringLiteral("No translated subtitle text is available.")
            : QStringLiteral("No source subtitle text is available.");
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(lines.join(QLatin1Char('\n')).toUtf8());
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool replaceCopy(const QString &source, const QString &destination, QString *error)
{
    if (source.isEmpty() || !QFileInfo(source).isFile()) return true;
    if (QFileInfo(source).absoluteFilePath().compare(
            QFileInfo(destination).absoluteFilePath(), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
        if (error) *error = QStringLiteral("Cannot replace package file: %1").arg(destination);
        return false;
    }
    if (!QFile::copy(source, destination)) {
        if (error) *error = QStringLiteral("Cannot copy %1 to %2.").arg(source, destination);
        return false;
    }
    return true;
}

} // namespace

DubbingController::DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                                     ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : DubbingController(sttSession, tts, nullptr, models, runtimes, parent)
{
}

DubbingController::~DubbingController() = default;

DubbingController::DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                                     TranslationEngine *translation,
                                     ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : QObject(parent), m_sttSession(sttSession), m_tts(tts),
      m_models(models), m_runtimes(runtimes)
{
    m_translation = translation;
    m_runner = new DubbingJobRunner(sttSession, tts, translation, models, runtimes, this);
    m_translationFix = new DubbingTranslationFixService(this);
    connect(m_translationFix, &DubbingTranslationFixService::stateChanged,
            this, [this]() {
        emit translationFixChanged();
        emit processingChanged();
        emit errorChanged();
        emit workflowChanged();
    });
    connect(m_translationFix, &DubbingTranslationFixService::completed,
            this, [this](const QVariantList &segments, int, int) {
        m_project.segments = segments;
        emit segmentsChanged();
        emit translationFixChanged();
        emit workflowChanged();
        persistAfterEdit();
    });
    connect(m_translationFix, &DubbingTranslationFixService::connectionTested,
            this, &DubbingController::translationFixConnectionTested);
    if (AppController::instance() && AppController::instance()->sessionRegistry()) {
        for (IModelSession *session : AppController::instance()->sessionRegistry()->sessions()) {
            if (!session) continue;
            connect(session, &IModelSession::stateChanged, this, [this]() {
                emit workflowChanged();
                scheduleAutomaticSetupAdvance();
            });
            connect(session, &IModelSession::activeConfigurationChanged, this, [this]() {
                emit workflowChanged();
                scheduleAutomaticSetupAdvance();
            });
        }
    }
    m_workflowRegistry = new NodeRegistry(this);
    registerDubbingWorkflowNodes(*m_workflowRegistry, m_runner);
    loadHistory();
    m_workflowRunner = new WorkflowGraphRunner(m_workflowRegistry, this);
    connect(m_workflowRunner, &WorkflowGraphRunner::stateChanged, this, [this]() {
        emit processingChanged();
        emit errorChanged();
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::nodeStarted, this,
            [this](const QString &nodeId) {
        const QString visibleStep = visibleStepForNode(nodeId);
        setCurrentStep(visibleStep);
        if (m_workflowMode == QStringLiteral("automatic")) {
            appendAutomaticEvent(QStringLiteral("Running %1").arg(visibleStep),
                                 QStringLiteral("running"), nodeId);
            setAutomaticStatus(QStringLiteral("Running node: %1").arg(visibleStep));
        }
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::nodeCompleted, this,
            [this](const QString &nodeId, const QVariantMap &) {
        if (m_workflowMode == QStringLiteral("automatic")) {
            appendAutomaticEvent(QStringLiteral("Completed %1").arg(visibleStepForNode(nodeId)),
                                 QStringLiteral("completed"), nodeId);
            if (nodeId == QStringLiteral("transcribe")) {
                if (auto *app = AppController::instance(); app && app->sessionRegistry()) {
                    if (IModelSession *stt = app->sessionRegistry()->sessionForCapability(
                            QStringLiteral("stt"))) {
                        const QList<SessionConfiguration> loaded = stt->loadedConfigurations();
                        for (const SessionConfiguration &configuration : loaded)
                            stt->requestUnloadConfiguration(configuration.signature);
                    }
                }
                appendAutomaticEvent(QStringLiteral("Releasing Whisper runtime before translation"),
                                     QStringLiteral("running"), QStringLiteral("transcribe"));
            } else if (nodeId == QStringLiteral("translate")) {
                if (auto *app = AppController::instance(); app && app->sessionRegistry()) {
                    if (IModelSession *translation = app->sessionRegistry()->sessionForCapability(
                            QStringLiteral("translation"))) {
                        const QList<SessionConfiguration> loaded = translation->loadedConfigurations();
                        for (const SessionConfiguration &configuration : loaded)
                            translation->requestUnloadConfiguration(configuration.signature);
                    }
                }
                prepareAutomaticVoiceRuntime();
            }
        }
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::reviewRequested, this, [this](const QVariantMap &request) {
        m_workflowReviewRequest = request;
        m_activeReviewId = request.value(QStringLiteral("reviewId")).toString();
        if (!m_project.projectPath.isEmpty() && !m_activeReviewId.isEmpty()) {
            if (!m_workflowReviewStore) {
                m_workflowReviewStore = std::make_unique<WorkflowReviewStore>(
                    QDir(QFileInfo(m_project.projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
            }
            WorkflowReviewRequest stored;
            stored.reviewId = m_activeReviewId;
            stored.runId = workflowRunId();
            stored.nodeRunId = workflowNodeRunId();
            stored.nodeId = m_workflowRunner->activeNodeId();
            stored.mode = request.value(QStringLiteral("mode")).toString();
            stored.editor = request.value(QStringLiteral("editor")).toString();
            stored.artifact = request.value(QStringLiteral("artifact"));
            stored.createdAt = QDateTime::currentDateTimeUtc();
            QString ignoredError;
            m_workflowReviewStore->save(stored, &ignoredError);
        }
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::completed, this, [this](const QVariantMap &) {
        if (m_workflowReviewStore && !m_activeReviewId.isEmpty()) m_workflowReviewStore->remove(m_activeReviewId);
        m_activeReviewId.clear();
        m_workflowReviewRequest.clear();
        setCurrentStep(QStringLiteral("completed"));
        if (m_workflowMode == QStringLiteral("automatic")) {
            setAutomaticStatus(QStringLiteral("Final dubbed media is ready"));
            appendAutomaticEvent(QStringLiteral("Final dubbed media is ready"),
                                 QStringLiteral("completed"), QStringLiteral("export"));
        }
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::failed, this, [this](const QString &) {
        if (m_workflowReviewStore && !m_activeReviewId.isEmpty()) m_workflowReviewStore->remove(m_activeReviewId);
        m_activeReviewId.clear();
        m_workflowReviewRequest.clear();
        setWorkflowMode(QStringLiteral("idle"));
        emit workflowChanged();
    });

    if (AppController::instance()) {
        if (auto *downloads = AppController::instance()->downloads()) {
            connect(downloads, &DownloadManager::activeDownloadsChanged,
                    this, &DubbingController::scheduleAutomaticSetupAdvance);
            connect(downloads, &DownloadManager::error, this,
                    [this](const QString &, const QString &, const QString &message) {
                if (m_automaticSetupActive) finishAutomaticSetupFailure(message);
            });
        }
        if (auto *install = AppController::instance()->downloadInstall()) {
            connect(install, &DownloadInstallService::installStatesChanged,
                    this, &DubbingController::scheduleAutomaticSetupAdvance);
        }
        if (auto *registry = AppController::instance()->sessionRegistry()) {
            const auto watchAutomaticLoad = [this, registry](const QString &capabilityId,
                                                              const QString &nodeId) {
                IModelSession *session = registry->sessionForCapability(capabilityId);
                if (!session) return;
                connect(session, &IModelSession::errorOccurred, this,
                        [this, capabilityId, nodeId](const QString &message) {
                    if (!m_automaticSetupActive) return;
                    const QVariantMap configuration =
                        m_workflowNodeConfigurations.value(nodeId).toMap();
                    const QString familyId = configuration.value(
                        QStringLiteral("familyId")).toString();
                    if (!familyId.isEmpty() && m_automaticConfiguredNodes.contains(nodeId)) {
                        appendAutomaticEvent(
                            QStringLiteral("Required default model %1 failed to load")
                                .arg(familyId),
                            QStringLiteral("failed"), nodeId);
                        finishAutomaticSetupFailure(
                            QStringLiteral("Failed to load required default model %1: %2")
                                .arg(familyId, message));
                        return;
                    }
                    finishAutomaticSetupFailure(
                        QStringLiteral("Failed to load %1 model: %2")
                            .arg(capabilityId, message));
                });
            };
            watchAutomaticLoad(QStringLiteral("stt"), QStringLiteral("transcribe"));
            watchAutomaticLoad(QStringLiteral("tts"), QStringLiteral("synthesize"));
        }
    }
    if (m_models)
        connect(m_models, &ModelManager::registryUpdated,
                this, &DubbingController::scheduleAutomaticSetupAdvance);
    if (m_runtimes)
        connect(m_runtimes, &RuntimeManager::registryUpdated,
                this, &DubbingController::scheduleAutomaticSetupAdvance);

    connect(m_runner, &DubbingJobRunner::stateChanged, this, [this]() {
        emit processingChanged();
        emit errorChanged();
        emit previewChanged();
        emit exportChanged();
        emit workflowChanged();
    });

    connect(m_runner, &DubbingJobRunner::errorOccurred, this, [this](const QString &) {
        m_pendingExportPath.clear();
    });

    connect(m_runner, &DubbingJobRunner::segmentsUpdated, this, [this](const QVariantList &segments) {
        m_project.segments = segments;
        emit segmentsChanged();
        emit workflowChanged();
        persistAfterEdit();
    });

    connect(m_runner, &DubbingJobRunner::segmentUpdated, this, [this](int index, const QVariantMap &patch) {
        if (index >= 0 && index < m_project.segments.size()) {
            m_project.segments[index] = patch;
            emit segmentsChanged();
            emit workflowChanged();
            persistAfterEdit();
        }
    });

    connect(m_runner, &DubbingJobRunner::ingestFinished, this, &DubbingController::onIngestFinished);
    connect(m_runner, &DubbingJobRunner::sourceSeparationFinished, this, [this](const QVariantMap &outputs) {
        m_project.analysisAudioPath = outputs.value(QStringLiteral("vocals"), m_project.masterAudioPath).toString();
        m_project.backgroundAudioPath = outputs.value(QStringLiteral("background"), m_project.masterAudioPath).toString();
        m_runner->setBackgroundAudioPath(m_project.backgroundAudioPath);
        emit projectChanged();
        emit workflowChanged();
        persistAfterEdit();
    });
    connect(m_runner, &DubbingJobRunner::stageCompleted, this,
            [this](const QString &nodeId, const QVariantMap &outputs) {
        m_stepOutputs.insert(nodeId, outputs);
        m_lastCompletedStepId = nodeId;
        if (nodeId == QStringLiteral("mix") && !m_pendingExportPath.isEmpty()) {
            const QString destination = m_pendingExportPath;
            m_pendingExportPath.clear();
            if (!m_runner->startExport(m_project.sourceMediaPath,
                                       outputs.value(QStringLiteral("audio")).toString(),
                                       destination, m_project.segments)) {
                emit workflowChanged();
                return;
            }
        }
        if (m_workflowMode == QStringLiteral("step")
            && (!m_workflowRunner || !m_workflowRunner->running())) {
            clearError();
            setAutomaticStatus(
                QStringLiteral("Manual node completed: %1").arg(visibleStepForNode(nodeId)));
            advanceManualStep(nodeId);
        }
        emit workflowChanged();
    });
}

bool DubbingController::processing() const
{
    return m_automaticSetupActive
        || (m_translationFix && m_translationFix->busy())
        || m_runner->processing()
        || (m_workflowRunner && m_workflowRunner->running());
}

QString DubbingController::stage() const
{
    if (m_automaticSetupActive) return QStringLiteral("model-setup");
    if (m_translationFix && m_translationFix->busy())
        return QStringLiteral("translation-fix");
    if (m_workflowRunner && m_workflowRunner->running()
        && !m_workflowRunner->activeNodeId().isEmpty())
        return m_workflowRunner->activeNodeId();
    return m_runner->stage();
}

int DubbingController::progress() const
{
    if (m_automaticSetupActive) {
        const auto *app = AppController::instance();
        const QVariantList downloads = app && app->downloads()
            ? app->downloads()->activeDownloads() : QVariantList();
        if (downloads.isEmpty()) return 5;
        qint64 received = 0;
        qint64 total = 0;
        for (const QVariant &entry : downloads) {
            const QVariantMap download = entry.toMap();
            received += download.value(QStringLiteral("bytesReceived")).toLongLong();
            total += download.value(QStringLiteral("bytesTotal")).toLongLong();
        }
        return total > 0 ? qBound(5, 5 + int(received * 15 / total), 20) : 8;
    }
    if (m_translationFix && m_translationFix->busy())
        return m_translationFix->progress();
    return m_workflowRunner && m_workflowRunner->running() ? m_workflowRunner->progress() : m_runner->progress();
}

bool DubbingController::settingsLocked() const
{
    return m_automaticSetupActive
        || (m_workflowMode == QStringLiteral("automatic") && processing());
}

QString DubbingController::lastError() const
{
    if (m_translationFix && !m_translationFix->lastError().isEmpty())
        return m_translationFix->lastError();
    return (m_workflowRunner && !m_workflowRunner->error().isEmpty()) ? m_workflowRunner->error() : m_runner->lastError();
}

bool DubbingController::translationFixing() const
{
    return m_translationFix && m_translationFix->busy();
}

int DubbingController::translationFixProgress() const
{
    return m_translationFix ? m_translationFix->progress() : 0;
}

QString DubbingController::translationFixStatus() const
{
    return m_translationFix ? m_translationFix->statusText() : QString();
}

QVariantMap DubbingController::translationFixConfiguration() const
{
    return m_translationFix ? m_translationFix->configuration() : QVariantMap();
}

int DubbingController::translationFixCandidateCount() const
{
    return DubbingTranslationFixService::eligibleSegmentCount(
        m_project.segments, m_project.targetLanguage);
}

QString DubbingController::adaptiveProvider() const
{
    return translationFixConfiguration().value(QStringLiteral("provider"),
                                               QStringLiteral("lmstudio")).toString();
}

bool DubbingController::adaptiveReady() const
{
    const QVariantMap config = translationFixConfiguration();
    if (!config.value(QStringLiteral("configured")).toBool()) return false;
    const QString provider = adaptiveProvider();
    if (provider == QStringLiteral("local")) {
        StudioConfiguration selection;
        selection.capabilityId = QStringLiteral("llm-chat");
        selection.familyId = config.value(QStringLiteral("model")).toString();
        selection.runtimeId = config.value(QStringLiteral("runtimeId")).toString();
        selection.runtimeVersion = config.value(QStringLiteral("runtimeVersion")).toString();
        selection.selectedFiles = config.value(QStringLiteral("selectedFiles")).toMap();
        return StudioConfigurationResolver::resolve(selection).isValid;
    }
    if (provider == QStringLiteral("cli")) {
        const QString cliAgent = config.value(QStringLiteral("cliAgent"), QStringLiteral("claude")).toString();
        return !DubbingTranslationFixService::cliExecutablePath(cliAgent).isEmpty();
    }
    return !config.value(QStringLiteral("serverUrl")).toString().trimmed().isEmpty()
        && !config.value(QStringLiteral("model")).toString().trimmed().isEmpty();
}

QString DubbingController::adaptiveStatusText() const
{
    if (!adaptiveReady()) return QStringLiteral("LLM setup required");
    const QString provider = adaptiveProvider();
    if (provider == QStringLiteral("local")) {
        return translationFixConfiguration()
            .value(QStringLiteral("model"), QStringLiteral("Local LLM ready")).toString();
    }
    if (provider == QStringLiteral("cli")) {
        const QString cliAgent = translationFixConfiguration().value(QStringLiteral("cliAgent"), QStringLiteral("claude")).toString();
        const QString model = translationFixConfiguration().value(QStringLiteral("model")).toString();
        QString label = QStringLiteral("Claude Code");
        if (cliAgent == QStringLiteral("codex")) label = QStringLiteral("Codex CLI");
        else if (cliAgent == QStringLiteral("antigravity")) label = QStringLiteral("Antigravity");
        return (model.isEmpty() || model == QStringLiteral("default"))
            ? QStringLiteral("%1 (CLI)").arg(label)
            : QStringLiteral("%1 · %2").arg(label, model);
    }
    const QString model = translationFixConfiguration().value(QStringLiteral("model")).toString();
    return provider == QStringLiteral("api")
        ? QStringLiteral("LLM API · %1").arg(model)
        : QStringLiteral("LM Studio · %1").arg(model);
}

QVariantMap DubbingController::firstCustomSetupIssue() const
{
    const QList<QPair<QString, QString>> requiredNodes{
        {QStringLiteral("source-separate"), QStringLiteral("voice-isolation")},
        {QStringLiteral("transcribe"), QStringLiteral("stt")},
        {QStringLiteral("translate"), QStringLiteral("translation")},
        {QStringLiteral("synthesize"), QStringLiteral("tts")}
    };
    for (const auto &required : requiredNodes) {
        const QVariantMap selected = m_workflowNodeConfigurations.value(required.first).toMap();
        if (selected.isEmpty()) {
            return {{QStringLiteral("nodeId"), required.first},
                    {QStringLiteral("setupKind"), QStringLiteral("node-model")},
                    {QStringLiteral("message"),
                     QStringLiteral("Choose a model for the %1 node before running Custom dubbing.")
                         .arg(visibleStepForNode(required.first))}};
        }
        StudioConfiguration configuration;
        configuration.capabilityId = required.second;
        configuration.familyId = selected.value(QStringLiteral("familyId")).toString();
        configuration.runtimeId = selected.value(QStringLiteral("runtimeId")).toString();
        configuration.runtimeVersion = selected.value(QStringLiteral("runtimeVersion")).toString();
        configuration.selectedFiles = selected.value(QStringLiteral("selectedFiles")).toMap();
        if (!StudioConfigurationResolver::resolve(configuration).isValid) {
            return {{QStringLiteral("nodeId"), required.first},
                    {QStringLiteral("setupKind"), QStringLiteral("node-model")},
                    {QStringLiteral("message"),
                     QStringLiteral("The selected model or runtime for %1 is not installed. Choose an available setup.")
                         .arg(visibleStepForNode(required.first))}};
        }
    }
    const bool rewriteEnabled =
        m_project.durationControl.value(QStringLiteral("enabled"), true).toBool()
        && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool();
    if (rewriteEnabled && !adaptiveReady()) {
        return {{QStringLiteral("nodeId"), QStringLiteral("translate")},
                {QStringLiteral("setupKind"), QStringLiteral("rewrite-model")},
                {QStringLiteral("message"),
                 QStringLiteral("Choose a local, CLI, or API rewrite model for the Translate node, or turn off automatic rewriting.")}};
    }
    return {};
}

bool DubbingController::customReady() const
{
    return firstCustomSetupIssue().isEmpty();
}

QString DubbingController::customStatusText() const
{
    const QVariantMap issue = firstCustomSetupIssue();
    if (!issue.isEmpty()) return issue.value(QStringLiteral("message")).toString();
    if (!m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool())
        return QStringLiteral("Custom models ready; long translations require manual review");
    return QStringLiteral("Custom models ready; rewrite: %1").arg(adaptiveStatusText());
}

QString DubbingController::previewPath() const
{
    return m_runner->previewPath();
}

QString DubbingController::dubbedVocalPath() const
{
    return m_runner->dubbedVocalPath();
}

QUrl DubbingController::playbackMediaUrl() const
{
    const QString exported = m_runner ? m_runner->exportPath() : QString();
    const QString suffix = QFileInfo(exported).suffix().toLower();
    if (!exported.isEmpty() && QFileInfo::exists(exported)
        && (suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
            || suffix == QStringLiteral("mov") || suffix == QStringLiteral("webm")
            || suffix == QStringLiteral("avi")))
        return QUrl::fromLocalFile(exported);
    return sourceMediaUrl();
}

QString DubbingController::exportPath() const
{
    return m_runner->exportPath();
}

QVariantList DubbingController::workflowNodes() const
{
    const bool hasMedia = !m_project.sourceMediaPath.trimmed().isEmpty();
    const bool hasSegments = !m_project.segments.isEmpty();
    bool hasTargets = false;
    bool allTargets = hasSegments;
    bool hasClips = false;
    bool hasConflict = false;
    for (const QVariant &entry : m_project.segments) {
        const QVariantMap segment = entry.toMap();
        hasTargets = hasTargets || !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
        allTargets = allTargets && !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
        hasClips = hasClips || (!segment.value(QStringLiteral("clipPath")).toString().isEmpty()
                                && QFileInfo::exists(segment.value(QStringLiteral("clipPath")).toString()));
        hasConflict = hasConflict || segment.value(QStringLiteral("timingConflict")).toBool();
    }
    const bool ttsReady = m_tts && m_tts->isModelLoaded();
    const bool translationReady = !m_project.targetLanguage.trimmed().isEmpty();
    const auto node = [](const QString &id, const QString &title, const QString &state,
                         const QString &detail, const QString &provider = QString()) {
        QVariantMap value{{QStringLiteral("id"), id}, {QStringLiteral("title"), title},
                          {QStringLiteral("state"), state}, {QStringLiteral("detail"), detail},
                          {QStringLiteral("providerName"), provider},
                          {QStringLiteral("providerState"), QStringLiteral("ready")}};
        return QVariant(value);
    };
    QVariantList result;
    const WorkflowGraph graph = DubbingWorkflowDefinition::create();
    for (const WorkflowGraphNode &definition : graph.nodes) {
        QString state = QStringLiteral("blocked");
        QString detail;
        QString provider;
        if (definition.id == QStringLiteral("media-input")) {
            state = hasMedia ? QStringLiteral("ready") : QStringLiteral("missing");
            detail = hasMedia ? QFileInfo(m_project.sourceMediaPath).fileName() : QStringLiteral("Import audio or video");
        } else if (definition.id == QStringLiteral("ingest")) {
            const bool normalized = !m_project.masterAudioPath.trimmed().isEmpty();
            state = normalized ? QStringLiteral("completed") : (hasMedia ? QStringLiteral("ready") : QStringLiteral("missing"));
            detail = normalized ? QStringLiteral("Media normalized") : (hasMedia ? QStringLiteral("Ready to normalize") : QStringLiteral("Import source media"));
        } else if (definition.id == QStringLiteral("source-separate")) {
            const bool separated = hasMedia && !m_project.backgroundAudioPath.trimmed().isEmpty();
            state = hasMedia ? (separated ? QStringLiteral("completed") : QStringLiteral("ready")) : QStringLiteral("missing");
            detail = separated ? QStringLiteral("Voice and background stems available")
                               : (hasMedia ? QStringLiteral("Use original audio if separation is unavailable") : QStringLiteral("Import source media"));
        } else if (definition.id == QStringLiteral("transcribe")) {
            const bool audioReady = !m_project.analysisAudioPath.trimmed().isEmpty() || !m_project.masterAudioPath.trimmed().isEmpty();
            state = hasSegments ? QStringLiteral("completed") : (audioReady ? QStringLiteral("ready") : QStringLiteral("blocked"));
            detail = hasSegments ? QStringLiteral("%1 segments").arg(m_project.segments.size()) : QStringLiteral("Speech-to-text source stage");
        } else if (definition.id == QStringLiteral("review-transcript")) {
            state = hasSegments ? QStringLiteral("completed") : QStringLiteral("blocked");
            detail = hasSegments ? QStringLiteral("Transcript available for review") : QStringLiteral("Transcribe source media first");
        } else if (definition.id == QStringLiteral("translate")) {
            state = !translationReady ? QStringLiteral("blocked") : (hasTargets ? QStringLiteral("completed") : (hasSegments ? QStringLiteral("ready") : QStringLiteral("missing")));
            detail = !translationReady ? QStringLiteral("Choose a target language") : (hasTargets ? QStringLiteral("Target text available") : QStringLiteral("Translate with CrispASR"));
            provider = QStringLiteral("CrispASR text translation");
        } else if (definition.id == QStringLiteral("review-translation")) {
            state = hasTargets ? QStringLiteral("completed") : QStringLiteral("blocked");
            detail = hasTargets ? QStringLiteral("Translated transcript available for review") : QStringLiteral("Translate the transcript first");
        } else if (definition.id == QStringLiteral("assign-voices")) {
            const bool voicesReady = hasTargets && !m_project.speakers.isEmpty();
            state = voicesReady ? QStringLiteral("ready") : QStringLiteral("blocked");
            detail = voicesReady ? QStringLiteral("Speaker assignments are ready") : QStringLiteral("Translated transcript and a speaker are required");
        } else if (definition.id == QStringLiteral("synthesize")) {
            state = !ttsReady ? QStringLiteral("missing") : (hasClips ? QStringLiteral("completed") : (allTargets ? QStringLiteral("ready") : QStringLiteral("blocked")));
            const QString defaultVoice = automaticDefaultFamilyId(
                QStringLiteral("tts"), m_project.dubbingQuality);
            detail = ttsReady ? QStringLiteral("TTS model loaded")
                              : (m_project.dubbingQuality == QStringLiteral("custom")
                                     ? QStringLiteral("Choose a TTS model")
                                     : QStringLiteral("Default: %1").arg(defaultVoice));
            provider = m_project.dubbingQuality == QStringLiteral("custom")
                ? QStringLiteral("No model configured")
                : (defaultVoice == QStringLiteral("vieneu-tts-v2-turbo")
                       ? QStringLiteral("VieNeu-TTS v2 Turbo (default)")
                       : QStringLiteral("OmniVoice (default)"));
        } else if (definition.id == QStringLiteral("fit-timing")) {
            state = !hasClips ? QStringLiteral("blocked") : (hasConflict ? QStringLiteral("blocked") : QStringLiteral("completed"));
            detail = hasConflict ? QStringLiteral("One or more clips exceed the fit tolerance") : QStringLiteral("Fit generated clips to segment timing");
        } else if (definition.id == QStringLiteral("review-conflicts")) {
            state = hasConflict ? QStringLiteral("blocked") : (hasClips ? QStringLiteral("completed") : QStringLiteral("blocked"));
            detail = hasConflict ? QStringLiteral("Review timing conflicts") : QStringLiteral("No timing conflicts pending");
        } else if (definition.id == QStringLiteral("mix")) {
            state = hasClips && !hasConflict ? QStringLiteral("ready") : QStringLiteral("blocked");
            detail = hasClips ? QStringLiteral("Mix generated clips with background audio") : QStringLiteral("Generate segment audio first");
        } else if (definition.id == QStringLiteral("export")) {
            state = !hasClips ? QStringLiteral("missing") : (!previewPath().isEmpty() ? QStringLiteral("completed") : QStringLiteral("ready"));
            detail = !hasClips ? QStringLiteral("Generate translated audio first") : (!previewPath().isEmpty() ? QStringLiteral("Preview rendered") : QStringLiteral("Render the mixed audio"));
        }
        if (workflowWaitingForInput() && definition.id == m_workflowRunner->activeNodeId()) {
            state = QStringLiteral("waiting_for_input");
            detail = QStringLiteral("Review is waiting for your decision");
        } else if (m_workflowRunner && m_workflowRunner->running()
                   && definition.id == m_workflowRunner->activeNodeId()) {
            state = QStringLiteral("running");
            detail = m_workflowMode != QStringLiteral("automatic")
                    || m_automaticStatusText.isEmpty()
                ? QStringLiteral("Node is running") : m_automaticStatusText;
        }
        QVariantMap item = node(definition.id, definition.title, state, detail, provider).toMap();
        item.insert(QStringLiteral("parameters"), definition.parameters);
        item.insert(QStringLiteral("typeId"), definition.typeId);
        item.insert(QStringLiteral("typeVersion"), definition.typeVersion);
        if (definition.id == QStringLiteral("source-separate")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("voice-isolation"));
        } else if (definition.id == QStringLiteral("transcribe")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("stt"));
        } else if (definition.id == QStringLiteral("translate")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("translation"));
        } else if (definition.id == QStringLiteral("synthesize")) {
            item.insert(QStringLiteral("configurable"), true);
            item.insert(QStringLiteral("capabilityId"), QStringLiteral("tts"));
        }
        if (item.value(QStringLiteral("configurable")).toBool()
            && m_project.dubbingQuality != QStringLiteral("custom")) {
            item.insert(
                QStringLiteral("defaultFamilyId"),
                automaticDefaultFamilyId(
                    item.value(QStringLiteral("capabilityId")).toString(),
                    m_project.dubbingQuality));
        }
        const QVariantMap selected = m_workflowNodeConfigurations.value(definition.id).toMap();
        if (!selected.isEmpty()) {
            item.insert(QStringLiteral("providerName"), selected.value(QStringLiteral("modelName")));
            item.insert(QStringLiteral("selectedFamilyId"), selected.value(QStringLiteral("familyId")));
            item.insert(QStringLiteral("selectedRuntimeId"), selected.value(QStringLiteral("runtimeId")));
            item.insert(QStringLiteral("supportsVoiceCloning"),
                        selected.value(QStringLiteral("supportsVoiceCloning")).toBool());
            const QString capabilityId = selected.value(QStringLiteral("capabilityId")).toString();
            IModelSession *session = AppController::instance() && AppController::instance()->sessionRegistry()
                ? AppController::instance()->sessionRegistry()->sessionForCapability(capabilityId) : nullptr;
            item.insert(QStringLiteral("providerState"),
                        session && session->canProcess() ? QStringLiteral("ready") : QStringLiteral("loading"));
            const ModelSessionState modelState = session ? session->state() : ModelSessionState::Unconfigured;
            item.insert(QStringLiteral("modelState"), static_cast<int>(modelState));
            item.insert(QStringLiteral("modelStateText"),
                        modelState == ModelSessionState::Ready ? QStringLiteral("ready")
                        : modelState == ModelSessionState::Loading ? QStringLiteral("loading")
                        : modelState == ModelSessionState::Unloading ? QStringLiteral("unloading")
                        : modelState == ModelSessionState::Processing ? QStringLiteral("processing")
                        : modelState == ModelSessionState::Error ? QStringLiteral("error")
                        : QStringLiteral("unloaded"));
            QVariantMap parameters = item.value(QStringLiteral("parameters")).toMap();
            const QVariantMap customParameters = selected.value(QStringLiteral("parameters")).toMap();
            for (auto it = customParameters.cbegin(); it != customParameters.cend(); ++it)
                parameters.insert(it.key(), it.value());
            item.insert(QStringLiteral("parameters"), parameters);
            QVariantList runtimeSchema;
            if (capabilityId == QStringLiteral("tts") && m_tts) {
                const QString signature = selected.value(QStringLiteral("configurationSignature")).toString();
                if (!signature.isEmpty() && m_tts->instance(signature))
                    runtimeSchema = m_tts->instance(signature)->schemaForCapability(QStringLiteral("tts"));
                else
                    runtimeSchema = m_tts->schemaForCapability(QStringLiteral("tts"));
            }
            const QVariantMap familyConfig = selected.value(QStringLiteral("familyConfig")).toMap();
            const QVariantMap studioConfig = familyConfig.value(QStringLiteral("studio")).toMap()
                .value(capabilityId).toMap();
            const QVariantList parameterSchema = CapabilitySettingsSchema::merge(
                familyConfig, capabilityId, runtimeSchema);
            item.insert(QStringLiteral("parameterSchema"), parameterSchema);
            item.insert(QStringLiteral("studioConfig"), studioConfig);
        }
        result.append(item);
    }
    return result;
}

bool DubbingController::workflowReady() const
{
    const bool sttReady = AppController::instance() && AppController::instance()->sessionRegistry()
        && AppController::instance()->sessionRegistry()->sessionForCapability(QStringLiteral("stt"))
        && AppController::instance()->sessionRegistry()->sessionForCapability(QStringLiteral("stt"))->canProcess();
    const bool translationConfigured = !m_workflowNodeConfigurations.value(QStringLiteral("translate")).toMap().isEmpty();
    bool translatedArtifactReady = !m_project.segments.isEmpty();
    for (const QVariant &entry : m_project.segments) {
        if (entry.toMap().value(QStringLiteral("targetText")).toString().trimmed().isEmpty()) {
            translatedArtifactReady = false;
            break;
        }
    }
    bool configuredTranslationReady = false;
    if (translationConfigured) {
        const QVariantMap selected = m_workflowNodeConfigurations
                                         .value(QStringLiteral("translate")).toMap();
        StudioConfiguration configuration;
        configuration.capabilityId = QStringLiteral("translation");
        configuration.familyId = selected.value(QStringLiteral("familyId")).toString();
        configuration.runtimeId = selected.value(QStringLiteral("runtimeId")).toString();
        configuration.runtimeVersion = selected.value(QStringLiteral("runtimeVersion")).toString();
        configuration.selectedFiles = selected.value(QStringLiteral("selectedFiles")).toMap();
        configuredTranslationReady = StudioConfigurationResolver::resolve(configuration).isValid;
    }
    const bool translationReady = !translationConfigured || translatedArtifactReady
        || configuredTranslationReady;
    return workflowGraphValid()
        && !m_project.sourceMediaPath.isEmpty()
        && !m_project.targetLanguage.trimmed().isEmpty()
        && m_tts && m_tts->isModelLoaded()
        && sttReady
        && translationReady;
}

bool DubbingController::setWorkflowNodeModel(const QString &nodeId,
                                             const QString &familyId,
                                             const QString &runtimeId,
                                             const QString &runtimeVersion,
                                             const QVariantMap &selectedFiles)
{
    return configureWorkflowNodeModel(nodeId, familyId, runtimeId, runtimeVersion,
                                      selectedFiles, true);
}

bool DubbingController::configureWorkflowNodeModel(const QString &nodeId,
                                                   const QString &familyId,
                                                   const QString &runtimeId,
                                                   const QString &runtimeVersion,
                                                   const QVariantMap &selectedFiles,
                                                   bool loadSession)
{
    QString capabilityId;
    if (nodeId == QStringLiteral("source-separate")) capabilityId = QStringLiteral("voice-isolation");
    else if (nodeId == QStringLiteral("transcribe")) capabilityId = QStringLiteral("stt");
    else if (nodeId == QStringLiteral("translate")) capabilityId = QStringLiteral("translation");
    else if (nodeId == QStringLiteral("synthesize")) capabilityId = QStringLiteral("tts");
    else {
        setError(QStringLiteral("This workflow node does not support model selection."));
        return false;
    }

    AppController *app = AppController::instance();
    if (!app || !app->registry() || !app->sessionRegistry()) return false;
    const QVariantList families = capabilityId == QStringLiteral("stt")
            || capabilityId == QStringLiteral("voice-isolation")
        ? app->registry()->sttFamilies()
        : (capabilityId == QStringLiteral("translation") ? app->registry()->translationFamilies()
                                                           : app->registry()->ttsFamilies());
    QVariantMap family;
    for (const QVariant &entry : families) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == familyId) {
            family = candidate;
            break;
        }
    }
    if (family.isEmpty()) {
        setError(QStringLiteral("The selected model family is not available."));
        return false;
    }

    QVariantMap runtime;
    for (const QVariant &entry : family.value(QStringLiteral("runtimes")).toList()) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == runtimeId) {
            runtime = candidate;
            break;
        }
    }
    if (runtime.isEmpty()) {
        setError(QStringLiteral("The selected runtime is not compatible with this model."));
        return false;
    }

    StudioConfiguration config;
    config.capabilityId = capabilityId;
    config.familyId = familyId;
    config.runtimeId = runtimeId;
    config.runtimeVersion = runtimeVersion.isEmpty()
        ? runtime.value(QStringLiteral("version")).toString() : runtimeVersion;
    for (const QVariant &entry : family.value(QStringLiteral("requiredFiles")).toList()) {
        const QVariantMap file = entry.toMap();
        const QString role = file.value(QStringLiteral("role")).toString();
        config.selectedFiles.insert(role, selectedFiles.value(role, file.value(QStringLiteral("file"))).toString());
    }
    const auto resolved = StudioConfigurationResolver::resolve(config);
    if (!resolved.isValid) {
        setError(QStringLiteral("The selected model files or runtime are not installed."));
        return false;
    }

    const bool supportsVoiceCloning = family.value(QStringLiteral("supportsCloning")).toBool()
        || family.value(QStringLiteral("capabilities")).toStringList().contains(QStringLiteral("voice-cloning"));
    QVariantMap parameters = m_workflowNodeConfigurations.value(nodeId).toMap()
                                 .value(QStringLiteral("parameters")).toMap();
    if (!supportsVoiceCloning)
        parameters.remove(QStringLiteral("autoSelectVoiceReference"));

    const bool isOmniVoice = familyId.contains(QStringLiteral("omnivoice"), Qt::CaseInsensitive);
    if (nodeId == QStringLiteral("synthesize") && isOmniVoice && supportsVoiceCloning) {
        if (!parameters.contains(QStringLiteral("autoSelectVoiceReference")))
            parameters.insert(QStringLiteral("autoSelectVoiceReference"), true);
        if (!parameters.contains(QStringLiteral("forceSegmentDuration")))
            parameters.insert(QStringLiteral("forceSegmentDuration"), true);
    }
    if (nodeId == QStringLiteral("synthesize")
        && !parameters.contains(QStringLiteral("lang"))) {
        parameters.insert(QStringLiteral("lang"), m_project.targetLanguage);
    }

    QVariantMap selected{{QStringLiteral("familyId"), familyId},
                         {QStringLiteral("runtimeId"), config.runtimeId},
                         {QStringLiteral("runtimeVersion"), config.runtimeVersion},
                          {QStringLiteral("selectedFiles"), config.selectedFiles},
                          {QStringLiteral("modelName"), family.value(QStringLiteral("title"))},
                          {QStringLiteral("capabilityId"), capabilityId},
                          {QStringLiteral("configurationSignature"), resolved.signature},
                          {QStringLiteral("supportsVoiceCloning"), supportsVoiceCloning},
                          {QStringLiteral("parameters"), parameters},
                          {QStringLiteral("familyConfig"), family},
                          {QStringLiteral("parameterDefinitions"), family.value(QStringLiteral("parameterDefinitions"))},
                          {QStringLiteral("studioConfig"),
                           family.value(QStringLiteral("studio")).toMap().value(capabilityId)}};
    m_workflowNodeConfigurations.insert(nodeId, selected);
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    else
        m_project.workflowNodeConfigurations.clear();
    if (loadSession) {
        unloadConflictingDubbingRuntime(app->sessionRegistry(), capabilityId);
        if (IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId)) {
            session->requestLoad(capabilityId, config);
        }
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Workflow node model changed node=%1 family=%2 runtime=%3")
                     .arg(nodeId, familyId, config.runtimeId));
    persistAfterEdit();
    emit workflowChanged();
    return true;
}

bool DubbingController::loadWorkflowNodeModel(const QString &nodeId)
{
    const QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    if (selected.isEmpty()) {
        setError(QStringLiteral("Choose a model configuration before loading this node."));
        return false;
    }
    return setWorkflowNodeModel(nodeId, selected.value(QStringLiteral("familyId")).toString(),
                                selected.value(QStringLiteral("runtimeId")).toString(),
                                selected.value(QStringLiteral("runtimeVersion")).toString(),
                                selected.value(QStringLiteral("selectedFiles")).toMap());
}

bool DubbingController::unloadWorkflowNodeModel(const QString &nodeId)
{
    const QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    const QString capabilityId = selected.value(QStringLiteral("capabilityId")).toString();
    auto *app = AppController::instance();
    auto *session = app && app->sessionRegistry()
        ? app->sessionRegistry()->sessionForCapability(capabilityId) : nullptr;
    if (!session || capabilityId.isEmpty()) return false;
    session->requestUnload(capabilityId);
    emit workflowChanged();
    return true;
}

bool DubbingController::reloadWorkflowNodeModel(const QString &nodeId)
{
    const QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    const QString capabilityId = selected.value(QStringLiteral("capabilityId")).toString();
    auto *app = AppController::instance();
    auto *session = app && app->sessionRegistry()
        ? app->sessionRegistry()->sessionForCapability(capabilityId) : nullptr;
    if (!session || capabilityId.isEmpty()) return false;
    session->requestReload(capabilityId);
    emit workflowChanged();
    return true;
}

bool DubbingController::setWorkflowNodeParameters(const QString &nodeId, const QVariantMap &parameters)
{
    if (nodeId.isEmpty()) return false;
    QVariantMap selected = m_workflowNodeConfigurations.value(nodeId).toMap();
    QVariantMap current = selected.value(QStringLiteral("parameters")).toMap();
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it)
        current.insert(it.key(), it.value());
    selected.insert(QStringLiteral("parameters"), current);
    m_workflowNodeConfigurations.insert(nodeId, selected);
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    else
        m_project.workflowNodeConfigurations.clear();
    persistAfterEdit();
    emit workflowChanged();
    return true;
}

QString DubbingController::workflowStatusText() const
{
    if (processing() && m_workflowMode == QStringLiteral("automatic")
        && !m_automaticStatusText.isEmpty()) return m_automaticStatusText;
    if (processing()) return QStringLiteral("Running %1 (%2%)").arg(stage()).arg(progress());
    if (workflowReady()) return QStringLiteral("Workflow configured and ready to run");
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        return customStatusText();
    return QStringLiteral("Configure media, transcript, target text, and a TTS model");
}

QString DubbingController::workflowId() const
{
    return QString::fromLatin1(DubbingWorkflowDefinition::Id);
}

int DubbingController::workflowVersion() const
{
    return DubbingWorkflowDefinition::Version;
}

bool DubbingController::workflowGraphValid() const
{
    if (!m_workflowRegistry) return false;
    return WorkflowGraphRunner(m_workflowRegistry).validate(DubbingWorkflowDefinition::create()).isEmpty();
}

QString DubbingController::workflowRunId() const
{
    if (m_workflowRunner && m_workflowRunner->running()) return m_workflowRunner->runId();
    return m_runner ? m_runner->runId() : QString();
}

QString DubbingController::workflowNodeRunId() const
{
    if (m_workflowRunner && m_workflowRunner->running()) return m_workflowRunner->nodeRunId();
    return m_runner ? m_runner->nodeRunId() : QString();
}

bool DubbingController::workflowWaitingForInput() const
{
    return m_workflowRunner && m_workflowRunner->waitingForInput();
}

QVariantMap DubbingController::workflowReviewRequest() const
{
    return m_workflowReviewRequest;
}

QString DubbingController::currentStepId() const
{
    if (m_workflowRunner && m_workflowRunner->running()) {
        return visibleStepForNode(m_workflowRunner->activeNodeId());
    }
    return m_currentStepId;
}

QVariantMap DubbingController::currentStepOutput() const
{
    return stepOutput(currentStepId());
}

QVariantMap DubbingController::stepOutput(const QString &stepId) const
{
    return m_stepOutputs.value(stepId).toMap();
}

void DubbingController::setWorkflowMode(const QString &mode)
{
    if (m_workflowMode == mode) return;
    m_workflowMode = mode;
    emit workflowChanged();
}

void DubbingController::setCurrentStep(const QString &stepId)
{
    if (m_currentStepId == stepId) return;
    m_currentStepId = stepId;
    emit workflowChanged();
}

void DubbingController::advanceManualStep(const QString &completedStepId)
{
    static const QHash<QString, QString> next{{QStringLiteral("ingest"), QStringLiteral("source-separate")},
                                              {QStringLiteral("source-separate"), QStringLiteral("transcribe")},
                                              {QStringLiteral("transcribe"), QStringLiteral("translate")},
                                              {QStringLiteral("translate"), QStringLiteral("synthesize")},
                                              {QStringLiteral("synthesize"), QStringLiteral("mix")},
                                              {QStringLiteral("mix"), QStringLiteral("export")},
                                              {QStringLiteral("export"), QStringLiteral("completed")}};
    if (next.contains(completedStepId)) setCurrentStep(next.value(completedStepId));
}

void DubbingController::prepareWorkflow()
{
    if (!workflowGraphValid()) {
        setError(QStringLiteral("The default dubbing workflow definition is invalid."));
        return;
    }
    emit workflowChanged();
}

QString DubbingController::defaultWorkflowModelFamily(const QString &nodeId) const
{
    QString capabilityId;
    if (nodeId == QStringLiteral("source-separate"))
        capabilityId = QStringLiteral("voice-isolation");
    else if (nodeId == QStringLiteral("transcribe"))
        capabilityId = QStringLiteral("stt");
    else if (nodeId == QStringLiteral("translate"))
        capabilityId = QStringLiteral("translation");
    else if (nodeId == QStringLiteral("synthesize"))
        capabilityId = QStringLiteral("tts");
    return automaticDefaultFamilyId(capabilityId, m_project.dubbingQuality);
}

void DubbingController::resetStandardWorkflowNodeModels()
{
    if (m_project.dubbingQuality == QStringLiteral("custom")) return;
    m_workflowNodeConfigurations.clear();
    m_project.workflowNodeConfigurations.clear();
    resetStandardTranslationFixConfiguration();
    emit workflowChanged();
}

void DubbingController::resetStandardTranslationFixConfiguration()
{
    if (!m_translationFix || m_translationFix->busy()) return;
    m_translationFix->setConfiguration({
        {QStringLiteral("provider"), QStringLiteral("lmstudio")},
        {QStringLiteral("configured"), false},
        {QStringLiteral("serverUrl"), QStringLiteral("http://127.0.0.1:1234")},
        {QStringLiteral("model"), QStringLiteral("qwen3.5-2b")},
        {QStringLiteral("maxAttempts"),
         m_project.durationControl.value(QStringLiteral("maxPreTtsIterations"), 4)},
        {QStringLiteral("temperature"), 0.35}
    });
    if (m_runner)
        m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
}

QString DubbingController::visibleStepForNode(const QString &nodeId)
{
    if (nodeId == QStringLiteral("media-input")) return QStringLiteral("import");
    if (nodeId == QStringLiteral("review-transcript")) return QStringLiteral("transcribe");
    if (nodeId == QStringLiteral("review-translation")) return QStringLiteral("translate");
    if (nodeId == QStringLiteral("assign-voices")) return QStringLiteral("synthesize");
    if (nodeId == QStringLiteral("fit-timing")
        || nodeId == QStringLiteral("review-conflicts")) return QStringLiteral("mix");
    return nodeId;
}

void DubbingController::setAutomaticStatus(const QString &message)
{
    if (m_automaticStatusText == message) return;
    m_automaticStatusText = message;
    emit workflowChanged();
}

void DubbingController::appendAutomaticEvent(const QString &message,
                                             const QString &state,
                                             const QString &nodeId)
{
    if (message.trimmed().isEmpty()) return;
    if (!m_automaticEvents.isEmpty()) {
        const QVariantMap last = m_automaticEvents.constLast().toMap();
        if (last.value(QStringLiteral("message")).toString() == message
            && last.value(QStringLiteral("state")).toString() == state)
            return;
    }
    m_automaticEvents.append(QVariantMap{
        {QStringLiteral("message"), message},
        {QStringLiteral("state"), state},
        {QStringLiteral("nodeId"), nodeId},
        {QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))}
    });
    while (m_automaticEvents.size() > 40) m_automaticEvents.removeFirst();
    emit workflowChanged();
}

CapabilityFamilyModel *DubbingController::automaticModel(const QString &capabilityId)
{
    AppController *app = AppController::instance();
    if (!app) return nullptr;
    std::unique_ptr<CapabilityFamilyModel> *holder = nullptr;
    if (capabilityId == QStringLiteral("stt")) holder = &m_automaticSttModel;
    else if (capabilityId == QStringLiteral("voice-isolation"))
        holder = &m_automaticVoiceIsolationModel;
    else if (capabilityId == QStringLiteral("translation")) holder = &m_automaticTranslationModel;
    else if (capabilityId == QStringLiteral("tts")) holder = &m_automaticTtsModel;
    else if (capabilityId == QStringLiteral("llm-chat")) holder = &m_automaticLlmModel;
    if (!holder) return nullptr;
    if (!*holder) {
        *holder = std::make_unique<CapabilityFamilyModel>(
            m_models, m_runtimes, app->registry(), app->settings(), this);
        (*holder)->setCapability(capabilityId);
    }
    (*holder)->refresh();
    return holder->get();
}

bool DubbingController::ensureAutomaticModel(const QString &nodeId,
                                             const QString &capabilityId,
                                             bool loadSession)
{
    CapabilityFamilyModel *model = automaticModel(capabilityId);
    AppController *app = AppController::instance();
    if (!model || !app || !app->downloadInstall() || !app->sessionRegistry()) {
        finishAutomaticSetupFailure(
            QStringLiteral("Automatic model setup is unavailable for %1.").arg(capabilityId));
        return false;
    }

    QVariantMap configuration = m_workflowNodeConfigurations.value(nodeId).toMap();
    QVariantMap recommendation;
    if (configuration.isEmpty()) {
        recommendation = model->configurationForFamily(
            automaticDefaultFamilyId(capabilityId, m_project.dubbingQuality));
        if (recommendation.isEmpty()) {
            finishAutomaticSetupFailure(
                QStringLiteral("The required default model %1 is not available for %2.")
                    .arg(automaticDefaultFamilyId(capabilityId, m_project.dubbingQuality),
                         capabilityId));
            return false;
        }
    } else {
        recommendation = {
            {QStringLiteral("familyId"), configuration.value(QStringLiteral("familyId"))},
            {QStringLiteral("runtimeId"), configuration.value(QStringLiteral("runtimeId"))},
            {QStringLiteral("runtimeVersion"), configuration.value(QStringLiteral("runtimeVersion"))},
            {QStringLiteral("selectedFiles"), configuration.value(QStringLiteral("selectedFiles"))}
        };
        model->setInitialSelectedFiles(recommendation.value(QStringLiteral("familyId")).toString(),
                                       recommendation.value(QStringLiteral("selectedFiles")).toMap());
        model->setSelectedFamilyId(recommendation.value(QStringLiteral("familyId")).toString());
        model->refresh();
    }

    const QString familyId = recommendation.value(QStringLiteral("familyId")).toString();
    QVariantMap familyItem = model->itemForFamily(familyId);
    if (familyItem.isEmpty()) {
        appendAutomaticEvent(
            QStringLiteral("Discarded stale %1 model setting: %2").arg(capabilityId, familyId),
            QStringLiteral("warning"), nodeId);
        m_workflowNodeConfigurations.remove(nodeId);
        m_automaticConfiguredNodes.remove(nodeId);
        scheduleAutomaticSetupAdvance();
        return false;
    }

    // A saved workflow may reference a model variant that has since been
    // removed from the compatibility catalog. Replace only those stale roles
    // with the currently supported selection so automatic runs can download
    // and use the compatible file without requiring manual reconfiguration.
    QVariantMap recommendedFiles = recommendation.value(QStringLiteral("selectedFiles")).toMap();
    const QVariantMap supportedFiles = familyItem.value(QStringLiteral("selectedFiles")).toMap();
    for (const QVariant &requirementValue : familyItem.value(QStringLiteral("requiredFiles")).toList()) {
        const QVariantMap requirement = requirementValue.toMap();
        const QString role = requirement.value(QStringLiteral("role")).toString();
        const QString selectedFile = recommendedFiles.value(role).toString();
        const QVariantList candidates = requirement.value(QStringLiteral("candidates")).toList();
        const QString defaultFile = requirement.value(QStringLiteral("file")).toString();
        const bool supported = candidates.isEmpty()
            ? selectedFile == defaultFile : candidates.contains(selectedFile);
        if (selectedFile.isEmpty() || !supported)
            recommendedFiles.insert(role, supportedFiles.value(role, defaultFile));
    }
    recommendation.insert(QStringLiteral("selectedFiles"), recommendedFiles);

    if (!familyItem.value(QStringLiteral("ready")).toBool()) {
        if (!m_automaticDownloadsQueued.contains(capabilityId)) {
            if (!app->downloadInstall()->enqueueRecommendedSetup(familyItem)) {
                finishAutomaticSetupFailure(
                    QStringLiteral("Could not start the %1 model download.").arg(capabilityId));
                return false;
            }
            m_automaticDownloadsQueued.insert(capabilityId);
            appendAutomaticEvent(
                QStringLiteral("Downloading the default %1 model and runtime").arg(capabilityId),
                QStringLiteral("downloading"), nodeId);
        }
        setAutomaticStatus(
            QStringLiteral("Preparing %1 model: %2").arg(capabilityId,
                familyItem.value(QStringLiteral("displayName"), familyId).toString()));
        return false;
    }

    m_automaticDownloadsQueued.remove(capabilityId);
    const QString runtimeId = recommendation.value(
        QStringLiteral("runtimeId"), familyItem.value(QStringLiteral("selectedRuntimeId"))).toString();
    const QString runtimeVersion = recommendation.value(
        QStringLiteral("runtimeVersion"), familyItem.value(QStringLiteral("selectedRuntimeVersion"))).toString();
    const QVariantMap selectedFiles = recommendation.value(
        QStringLiteral("selectedFiles"), familyItem.value(QStringLiteral("selectedFiles"))).toMap();

    if (!loadSession) {
        StudioConfiguration selected;
        selected.capabilityId = capabilityId;
        selected.familyId = familyId;
        selected.runtimeId = runtimeId;
        selected.runtimeVersion = runtimeVersion;
        selected.selectedFiles = selectedFiles;
        if (!StudioConfigurationResolver::resolve(selected).isValid) return false;
        if (configuration.isEmpty()) {
            if (!configureWorkflowNodeModel(nodeId, familyId, runtimeId,
                                            runtimeVersion, selectedFiles, false))
                return false;
            m_automaticConfiguredNodes.insert(nodeId);
        }
        return true;
    }

    IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId);
    if (session && session->canProcess()) return true;
    if (session && (session->state() == ModelSessionState::Loading
                    || session->state() == ModelSessionState::Processing)) {
        setAutomaticStatus(QStringLiteral("Loading %1 model into memory").arg(capabilityId));
        return false;
    }
    setAutomaticStatus(QStringLiteral("Loading %1 model into memory").arg(capabilityId));
    const bool automaticallySelected = configuration.isEmpty();
    if (automaticallySelected) m_automaticConfiguredNodes.insert(nodeId);
    const bool configured = configureWorkflowNodeModel(
        nodeId, familyId, runtimeId, runtimeVersion, selectedFiles, true);
    if (!configured && automaticallySelected) m_automaticConfiguredNodes.remove(nodeId);
    return configured && session && session->canProcess();
}

bool DubbingController::ensureAutomaticAdaptiveModel()
{
    if (m_project.dubbingQuality == QStringLiteral("custom")) {
        const bool rewriteEnabled =
            m_project.durationControl.value(QStringLiteral("enabled"), true).toBool()
            && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool();
        if (!rewriteEnabled || adaptiveReady()) return true;
        finishAutomaticSetupFailure(
            QStringLiteral("The Translate node requires a rewrite model for Custom dubbing."));
        return false;
    }
    if (m_project.dubbingQuality != QStringLiteral("adaptive")) return true;
    if (adaptiveReady()) return true;
    CapabilityFamilyModel *model = automaticModel(QStringLiteral("llm-chat"));
    AppController *app = AppController::instance();
    if (!model || !app || !app->downloadInstall()) {
        finishAutomaticSetupFailure(QStringLiteral("Automatic Adaptive model setup is unavailable."));
        return false;
    }
    const QVariantMap recommendation = model->configurationForFamily(
        automaticDefaultFamilyId(QStringLiteral("llm-chat"),
                                 m_project.dubbingQuality));
    if (recommendation.isEmpty()) {
        finishAutomaticSetupFailure(QStringLiteral("No compatible local LLM is available for Adaptive quality."));
        return false;
    }
    const QString familyId = recommendation.value(QStringLiteral("familyId")).toString();
    const QVariantMap item = model->itemForFamily(familyId);
    if (!item.value(QStringLiteral("ready")).toBool()) {
        if (!m_automaticDownloadsQueued.contains(QStringLiteral("llm-chat"))) {
            if (!app->downloadInstall()->enqueueRecommendedSetup(item)) {
                finishAutomaticSetupFailure(QStringLiteral("Could not start the Adaptive LLM download."));
                return false;
            }
            m_automaticDownloadsQueued.insert(QStringLiteral("llm-chat"));
            appendAutomaticEvent(QStringLiteral("Downloading the default Adaptive LLM"),
                                 QStringLiteral("downloading"), QStringLiteral("translate"));
        }
        setAutomaticStatus(QStringLiteral("Preparing Adaptive quality model: %1")
                               .arg(item.value(QStringLiteral("displayName"), familyId).toString()));
        return false;
    }
    QVariantMap configuration{
        {QStringLiteral("provider"), QStringLiteral("local")},
        {QStringLiteral("configured"), true},
        {QStringLiteral("model"), familyId},
        {QStringLiteral("runtimeId"), recommendation.value(QStringLiteral("runtimeId"))},
        {QStringLiteral("runtimeVersion"), recommendation.value(QStringLiteral("runtimeVersion"))},
        {QStringLiteral("selectedFiles"), recommendation.value(QStringLiteral("selectedFiles"))},
        {QStringLiteral("maxAttempts"), m_project.durationControl.value(QStringLiteral("maxPreTtsIterations"), 4)},
        {QStringLiteral("temperature"), 0.35}
    };
    m_translationFix->setConfiguration(configuration);
    m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
    appendAutomaticEvent(QStringLiteral("Adaptive LLM is configured"),
                         QStringLiteral("completed"), QStringLiteral("translate"));
    return true;
}

void DubbingController::scheduleAutomaticSetupAdvance()
{
    if (!m_automaticSetupActive || m_automaticAdvanceScheduled) return;
    m_automaticAdvanceScheduled = true;
    QTimer::singleShot(100, this, [this]() {
        m_automaticAdvanceScheduled = false;
        advanceAutomaticSetup();
    });
}

void DubbingController::prepareAutomaticVoiceRuntime()
{
    if (m_workflowMode != QStringLiteral("automatic")
        || !m_workflowRunner || !m_workflowRunner->running())
        return;
    AppController *app = AppController::instance();
    if (!app || !app->sessionRegistry()) return;
    for (const QString &capabilityId : {QStringLiteral("stt"),
                                        QStringLiteral("translation")}) {
        IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId);
        if (session && (session->state() == ModelSessionState::Loading
                        || session->state() == ModelSessionState::Processing
                        || session->state() == ModelSessionState::Unloading
                        || !session->loadedConfigurations().isEmpty())) {
            QTimer::singleShot(100, this, &DubbingController::prepareAutomaticVoiceRuntime);
            return;
        }
    }
    IModelSession *tts = app->sessionRegistry()->sessionForCapability(QStringLiteral("tts"));
    if (tts && (tts->canProcess() || tts->state() == ModelSessionState::Loading)) return;
    setAutomaticStatus(QStringLiteral("Loading the selected model for the Voice node"));
    appendAutomaticEvent(QStringLiteral("Loading the selected voice generation model"),
                         QStringLiteral("loading"), QStringLiteral("synthesize"));
    if (!loadWorkflowNodeModel(QStringLiteral("synthesize"))) {
        setError(QStringLiteral("Could not load the selected model for the Voice node."));
        if (m_workflowRunner->running()) m_workflowRunner->cancel();
    }
}

void DubbingController::finishAutomaticSetupFailure(const QString &message)
{
    if (!m_automaticSetupActive) return;
    m_automaticSetupActive = false;
    m_automaticOutputPath.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticConfiguredNodes.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setError(message);
    setAutomaticStatus(message);
    appendAutomaticEvent(message, QStringLiteral("failed"));
    emit processingChanged();
    emit workflowChanged();
}

void DubbingController::advanceAutomaticSetup()
{
    if (!m_automaticSetupActive) return;
    if (auto *app = AppController::instance(); app && app->sessionRegistry()) {
        bool waitingForRelease = false;
        for (const QString &capabilityId : {QStringLiteral("tts"),
                                            QStringLiteral("translation"),
                                            QStringLiteral("llm-chat")}) {
            IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId);
            if (!session) continue;
            const QList<SessionConfiguration> loaded = session->loadedConfigurations();
            for (const SessionConfiguration &configuration : loaded)
                session->requestUnloadConfiguration(configuration.signature);
            waitingForRelease = waitingForRelease || !loaded.isEmpty()
                || session->state() == ModelSessionState::Loading
                || session->state() == ModelSessionState::Processing
                || session->state() == ModelSessionState::Unloading;
        }
        if (waitingForRelease) {
            setAutomaticStatus(QStringLiteral("Releasing previously loaded native runtimes"));
            scheduleAutomaticSetupAdvance();
            return;
        }
    }
    if (!ensureAutomaticModel(QStringLiteral("source-separate"),
                              QStringLiteral("voice-isolation"), false)) return;
    appendAutomaticEvent(QStringLiteral("Voice isolation model is ready"),
                         QStringLiteral("completed"), QStringLiteral("source-separate"));
    if (!ensureAutomaticModel(QStringLiteral("transcribe"), QStringLiteral("stt"), true)) return;
    appendAutomaticEvent(QStringLiteral("Speech-to-text model is ready"),
                         QStringLiteral("completed"), QStringLiteral("transcribe"));
    if (!ensureAutomaticModel(QStringLiteral("translate"), QStringLiteral("translation"), false)) return;
    appendAutomaticEvent(QStringLiteral("Translation model is ready"),
                         QStringLiteral("completed"), QStringLiteral("translate"));
    if (!ensureAutomaticModel(QStringLiteral("synthesize"), QStringLiteral("tts"), false)) return;
    appendAutomaticEvent(QStringLiteral("Voice generation model is configured"),
                         QStringLiteral("completed"), QStringLiteral("synthesize"));
    if (!ensureAutomaticAdaptiveModel()) return;

    const QString outputPath = m_automaticOutputPath;
    m_automaticSetupActive = false;
    m_automaticDownloadsQueued.clear();
    m_automaticConfiguredNodes.clear();
    setAutomaticStatus(QStringLiteral("Models ready. Starting the dubbing workflow."));
    appendAutomaticEvent(QStringLiteral("All required models are ready"),
                         QStringLiteral("completed"));
    emit processingChanged();
    emit workflowChanged();
    if (m_runner) m_runner->setTranslationFixConfiguration(translationFixConfiguration());
    setCurrentStep(QStringLiteral("ingest"));
    if (!runWorkflow(outputPath)) {
        setWorkflowMode(QStringLiteral("idle"));
        setAutomaticStatus(lastError());
        appendAutomaticEvent(lastError(), QStringLiteral("failed"));
    }
}

bool DubbingController::runWorkflow(const QString &outputPath)
{
    if (!m_workflowRunner || m_workflowRunner->running()) return false;
    if (PathUtils::urlToLocalPath(outputPath).trimmed().isEmpty()) {
        setError(QStringLiteral("Choose an output path before running the full dubbing workflow."));
        return false;
    }
    if (!workflowGraphValid() || m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import source media before running the dubbing workflow."));
        return false;
    }
    WorkflowGraph graph = DubbingWorkflowDefinition::create();
    QVariantMap effectiveDurationControl = m_project.durationControl;
    effectiveDurationControl.insert(
        QStringLiteral("autoRewrite"),
        m_project.dubbingQuality != QStringLiteral("fast")
            && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool());
    for (auto &node : graph.nodes) {
        if (node.id == QStringLiteral("translate"))
            node.parameters.insert(QStringLiteral("durationControl"), effectiveDurationControl);
        const QVariantMap modelConfig = m_workflowNodeConfigurations.value(node.id).toMap();
        if (!modelConfig.isEmpty()) {
            node.parameters.insert(QStringLiteral("familyId"), modelConfig.value(QStringLiteral("familyId")));
            node.parameters.insert(QStringLiteral("runtimeId"), modelConfig.value(QStringLiteral("runtimeId")));
            node.parameters.insert(QStringLiteral("runtimeVersion"), modelConfig.value(QStringLiteral("runtimeVersion")));
            node.parameters.insert(QStringLiteral("selectedFiles"), modelConfig.value(QStringLiteral("selectedFiles")));
            const QVariantMap customParameters = modelConfig.value(QStringLiteral("parameters")).toMap();
            for (auto it = customParameters.cbegin(); it != customParameters.cend(); ++it)
                node.parameters.insert(it.key(), it.value());
            node.properties = node.parameters;
        }
        if (node.id == QStringLiteral("media-input")) {
            node.parameters.insert(QStringLiteral("value"), m_project.sourceMediaPath);
            node.properties.insert(QStringLiteral("value"), m_project.sourceMediaPath);
        } else if (node.id == QStringLiteral("transcribe")) {
            node.parameters.insert(QStringLiteral("language"),
                                   m_project.sourceLanguage.trimmed().isEmpty()
                                       ? QStringLiteral("auto") : m_project.sourceLanguage);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("translate")) {
            node.parameters.insert(QStringLiteral("sourceLanguage"), m_project.sourceLanguage);
            node.parameters.insert(QStringLiteral("targetLanguage"), m_project.targetLanguage);
            node.properties = node.parameters;
        } else if (node.typeId == QStringLiteral("core.review-gate")) {
            node.parameters.insert(QStringLiteral("mode"), QStringLiteral("never"));
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("synthesize")) {
            QVariantMap synthesisSettings = modelConfig.value(QStringLiteral("parameters")).toMap();
            synthesisSettings.insert(QStringLiteral("familyId"),
                                     modelConfig.value(QStringLiteral("familyId")));
            synthesisSettings.insert(QStringLiteral("autoReferenceSourcePath"),
                                     !m_project.analysisAudioPath.isEmpty()
                                         ? m_project.analysisAudioPath : m_project.masterAudioPath);
            if (!synthesisSettings.contains(QStringLiteral("lang")))
                synthesisSettings.insert(QStringLiteral("lang"), m_project.targetLanguage);
            node.parameters.insert(QStringLiteral("projectPath"), m_project.projectPath);
            node.parameters.insert(QStringLiteral("synthesisSettings"), synthesisSettings);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("fit-timing")) {
            node.parameters.insert(QStringLiteral("projectPath"), m_project.projectPath);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("mix")) {
            node.parameters.insert(QStringLiteral("projectPath"), m_project.projectPath);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("export")) {
            node.parameters.insert(QStringLiteral("destination"), PathUtils::urlToLocalPath(outputPath));
            node.properties = node.parameters;
        }
    }
    m_workflowJournal = std::make_unique<WorkflowRunJournal>(
        QDir(QFileInfo(m_project.projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
    m_workflowRunner->setJournal(m_workflowJournal.get());
    return m_workflowRunner->run(graph);
}

bool DubbingController::startAutomaticWorkflow(const QString &outputPath)
{
    if (processing()) return false;
    const QString destination = PathUtils::urlToLocalPath(outputPath).trimmed();
    if (destination.isEmpty()) {
        setError(QStringLiteral("Choose an output path before generating the final dub."));
        return false;
    }
    if (!workflowGraphValid() || m_project.sourceMediaPath.trimmed().isEmpty()) {
        setError(QStringLiteral("Import source media before generating the final dub."));
        return false;
    }
    if (m_project.dubbingQuality == QStringLiteral("custom")) {
        const QVariantMap issue = firstCustomSetupIssue();
        if (!issue.isEmpty()) {
            const QString message = issue.value(QStringLiteral("message")).toString();
            setError(message);
            emit workflowSetupRequired(
                issue.value(QStringLiteral("nodeId")).toString(),
                issue.value(QStringLiteral("setupKind")).toString(), message);
            emit workflowChanged();
            return false;
        }
    }
    clearError();
    setWorkflowMode(QStringLiteral("automatic"));
    setCurrentStep(QStringLiteral("import"));
    m_automaticOutputPath = destination;
    m_automaticSetupActive = true;
    m_automaticEvents.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticConfiguredNodes.clear();
    setAutomaticStatus(QStringLiteral("Checking required models and runtimes"));
    appendAutomaticEvent(QStringLiteral("Checking required models and runtimes"),
                         QStringLiteral("running"));
    emit processingChanged();
    emit workflowChanged();
    scheduleAutomaticSetupAdvance();
    return true;
}

void DubbingController::pauseAutomaticWorkflow()
{
    if (m_workflowMode != QStringLiteral("automatic") || !processing()) return;
    m_automaticSetupActive = false;
    m_automaticOutputPath.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticConfiguredNodes.clear();
    if (m_translationFix) m_translationFix->cancel();
    if (m_workflowRunner && m_workflowRunner->running()) m_workflowRunner->cancel();
    if (m_runner) m_runner->cancel();
    setWorkflowMode(QStringLiteral("paused"));
    setAutomaticStatus(QStringLiteral("Paused. Settings are unlocked; Generate resumes the workflow."));
    appendAutomaticEvent(QStringLiteral("Automatic generation paused"),
                         QStringLiteral("paused"), currentStepId());
    emit processingChanged();
    emit workflowChanged();
}

void DubbingController::startStepByStep()
{
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Step-by-step requested current=%1 processing=%2 source=%3 master=%4 background=%5 segments=%6")
                     .arg(m_currentStepId)
                     .arg(processing() ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(m_project.sourceMediaPath, m_project.masterAudioPath,
                          m_project.backgroundAudioPath)
                     .arg(m_project.segments.size()));
    if (m_project.sourceMediaPath.isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Step-by-step rejected: source media is empty."));
        setError(QStringLiteral("Import source media before starting the step-by-step workflow."));
        return;
    }
    setWorkflowMode(QStringLiteral("step"));
    if (m_project.masterAudioPath.isEmpty()) setCurrentStep(QStringLiteral("ingest"));
    else if (m_project.backgroundAudioPath.isEmpty()) setCurrentStep(QStringLiteral("source-separate"));
    else if (m_project.segments.isEmpty()) setCurrentStep(QStringLiteral("transcribe"));
    else {
        bool allTranslated = true;
        bool allGenerated = true;
        for (const QVariant &entry : m_project.segments) {
            const QVariantMap segment = entry.toMap();
            allTranslated = allTranslated && !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
            allGenerated = allGenerated && QFileInfo::exists(segment.value(QStringLiteral("clipPath")).toString());
        }
        if (!allTranslated) setCurrentStep(QStringLiteral("translate"));
        else if (!allGenerated) setCurrentStep(QStringLiteral("synthesize"));
        else if (previewPath().isEmpty() || !QFileInfo::exists(previewPath())) setCurrentStep(QStringLiteral("mix"));
        else if (exportPath().isEmpty() || !QFileInfo::exists(exportPath())) setCurrentStep(QStringLiteral("export"));
        else setCurrentStep(QStringLiteral("completed"));
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Step-by-step resolved next step=%1").arg(m_currentStepId));
}

bool DubbingController::runCurrentStep(const QString &outputPath)
{
    if (m_workflowMode != QStringLiteral("step")) startStepByStep();
    if (processing()) return false;
    const QString step = m_currentStepId;
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Run current step step=%1 mode=%2 output=%3 project=%4")
                     .arg(step, m_workflowMode, outputPath, m_project.projectPath));
    if (step == QStringLiteral("ingest")) {
        m_runner->startIngest(m_project.sourceMediaPath);
        return m_runner->processing();
    }
    if (step == QStringLiteral("source-separate")) {
        m_runner->startSourceSeparation(
            m_project.masterAudioPath,
            m_workflowNodeConfigurations.value(QStringLiteral("source-separate")).toMap());
        return m_runner->processing() || !m_project.masterAudioPath.isEmpty();
    }
    if (step == QStringLiteral("transcribe")) {
        transcribeSource();
        return m_runner->processing();
    }
    if (step == QStringLiteral("translate")) {
        translateSource();
        return m_runner->processing();
    }
    if (step == QStringLiteral("synthesize")) {
        generateAudio();
        return m_runner->processing();
    }
    if (step == QStringLiteral("mix")) return renderPreview();
    if (step == QStringLiteral("export")) return exportMedia(outputPath);
    return false;
}

bool DubbingController::rerunStep(const QString &stepId, const QString &outputPath)
{
    if (processing()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Run request rejected while busy requestedStep=%1 activeStep=%2 runnerStage=%3 progress=%4")
                            .arg(stepId, m_currentStepId, m_runner->stage())
                            .arg(m_runner->progress()));
        return false;
    }

    const QString step = stepId.trimmed();
    const bool supported = step == QStringLiteral("ingest")
        || step == QStringLiteral("source-separate")
        || step == QStringLiteral("transcribe")
        || step == QStringLiteral("translate")
        || step == QStringLiteral("synthesize")
        || step == QStringLiteral("mix")
        || step == QStringLiteral("export");
    if (!supported) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Ignoring rerun request for unsupported step=%1").arg(step));
        return false;
    }
    if (m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import source media before running this step again."));
        return false;
    }

    clearError();
    m_automaticEvents.clear();
    setWorkflowMode(QStringLiteral("step"));
    setCurrentStep(step);
    setAutomaticStatus(QStringLiteral("Running manual node: %1").arg(visibleStepForNode(step)));
    appendAutomaticEvent(QStringLiteral("Running manual node: %1").arg(visibleStepForNode(step)),
                         QStringLiteral("running"), step);
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Rerun step step=%1 output=%2 project=%3")
                     .arg(step, outputPath, m_project.projectPath));
    return runCurrentStep(outputPath);
}

bool DubbingController::approveWorkflowReview(const QVariantMap &artifact)
{
    if (!workflowWaitingForInput()) return false;
    return m_workflowRunner->resume(QVariantMap{{QStringLiteral("action"), QStringLiteral("approve")},
                                                 {QStringLiteral("artifact"), artifact.isEmpty()
                                                     ? m_workflowReviewRequest.value(QStringLiteral("artifact")) : QVariant(artifact)}});
}

bool DubbingController::rejectWorkflowReview(const QString &reason)
{
    if (!workflowWaitingForInput()) return false;
    return m_workflowRunner->resume(QVariantMap{{QStringLiteral("action"), QStringLiteral("reject")},
                                                 {QStringLiteral("reason"), reason}});
}

void DubbingController::setSourceLanguage(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty() || normalized == m_project.sourceLanguage) return;
    m_project.sourceLanguage = normalized;
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setTargetLanguage(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty() || normalized == m_project.targetLanguage) return;
    m_project.targetLanguage = normalized;

    QVariantMap synthesis = m_workflowNodeConfigurations
                                .value(QStringLiteral("synthesize")).toMap();
    if (!synthesis.isEmpty()) {
        QVariantMap parameters = synthesis.value(QStringLiteral("parameters")).toMap();
        parameters.insert(QStringLiteral("lang"), normalized);
        synthesis.insert(QStringLiteral("parameters"), parameters);
        m_workflowNodeConfigurations.insert(QStringLiteral("synthesize"), synthesis);
    }

    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setDurationControl(const QVariantMap &value)
{
    m_project.durationControl = value;
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setDubbingQuality(const QString &value)
{
    const QString requested = value.trimmed().toLower();
    const QString normalized =
        requested == QStringLiteral("adaptive") || requested == QStringLiteral("custom")
        ? requested : QStringLiteral("fast");
    if (normalized == m_project.dubbingQuality) return;
    m_project.dubbingQuality = normalized;
    if (normalized != QStringLiteral("custom")) {
        m_workflowNodeConfigurations.clear();
        m_project.workflowNodeConfigurations.clear();
        resetStandardTranslationFixConfiguration();
    } else if (m_translationFix
               && !m_project.customRewriteConfiguration.isEmpty()) {
        m_translationFix->setConfiguration(m_project.customRewriteConfiguration);
        if (m_runner)
            m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
    }
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

bool DubbingController::ensureProject(const QString &path)
{
    if (!path.trimmed().isEmpty()) {
        m_project.projectPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    }
    if (m_project.projectPath.isEmpty()) {
        setError(QStringLiteral("Choose a project file before saving."));
        return false;
    }
    return true;
}

bool DubbingController::newProject(const QString &path)
{
    m_project = DubbingProject();
    m_workflowNodeConfigurations.clear();
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(QStringLiteral("import"));
    if (m_workflowRunner) m_workflowRunner->setJournal(nullptr);
    m_workflowJournal.reset();
    m_workflowReviewStore.reset();
    m_activeReviewId.clear();
    m_workflowReviewRequest.clear();
    if (!path.isEmpty()) {
        if (!ensureProject(path)) return false;
    } else {
        m_project.projectPath = PathUtils::dataDir() + QStringLiteral("/dubbing/untitled.ladub.json");
    }
    m_project.speakers.append(QVariantMap{{QStringLiteral("id"), QStringLiteral("speaker-1")},
                                          {QStringLiteral("name"), QStringLiteral("Speaker 1")},
                                          {QStringLiteral("voice"), QVariantMap()} });
    emit projectChanged();
    emit segmentsChanged();
    emit workflowChanged();
    return saveProject();
}

bool DubbingController::openProject(const QString &path)
{
    DubbingProject loaded;
    QString error;
    if (!DubbingProject::load(PathUtils::urlToLocalPath(path), loaded, &error)) {
        setError(error);
        return false;
    }
    m_project = std::move(loaded);
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        m_workflowNodeConfigurations = m_project.workflowNodeConfigurations;
    else {
        m_workflowNodeConfigurations.clear();
        m_project.workflowNodeConfigurations.clear();
        resetStandardTranslationFixConfiguration();
    }
    if (m_project.dubbingQuality == QStringLiteral("custom")
        && m_translationFix && !m_project.customRewriteConfiguration.isEmpty()) {
        m_translationFix->setConfiguration(m_project.customRewriteConfiguration);
        if (m_runner)
            m_runner->setTranslationFixConfiguration(m_translationFix->configuration());
    }
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(m_project.sourceMediaPath.isEmpty() ? QStringLiteral("import") : QStringLiteral("ingest"));
    if (m_workflowRunner) m_workflowRunner->setJournal(nullptr);
    m_workflowJournal.reset();
    m_workflowReviewStore.reset();
    m_activeReviewId.clear();
    m_workflowReviewRequest.clear();
    
    // Sync paths to runner
    m_runner->setPreviewPath(QFileInfo(m_project.projectPath).absolutePath() + QStringLiteral("/preview.wav"));
    m_runner->setExportPath(QString());
    
    emit projectChanged();
    emit segmentsChanged();
    emit workflowChanged();
    return true;
}

bool DubbingController::saveProject()
{
    if (!ensureProject(QString())) return false;
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        m_project.workflowNodeConfigurations = m_workflowNodeConfigurations;
    else
        m_project.workflowNodeConfigurations.clear();
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        m_project.customRewriteConfiguration = translationFixConfiguration();
    QString error;
    if (!m_project.save(&error)) {
        setError(error);
        return false;
    }
    recordHistoryEntry();
    return true;
}

QString DubbingController::historyPath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + QStringLiteral("/history");
    QDir().mkpath(base);
    return base + QStringLiteral("/dubbing_history.json");
}

void DubbingController::loadHistory()
{
    QFile file(historyPath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (document.isArray()) {
        m_history = document.array().toVariantList();
        emit historyChanged();
    }
}

void DubbingController::recordHistoryEntry()
{
    if (m_project.projectPath.isEmpty()) return;
    const QFileInfo projectInfo(m_project.projectPath);
    QVariantList updated;
    for (const QVariant &value : std::as_const(m_history)) {
        if (value.toMap().value(QStringLiteral("projectPath")).toString()
            != projectInfo.absoluteFilePath())
            updated.append(value);
    }
    updated.prepend(QVariantMap{
        {QStringLiteral("id"), projectInfo.absoluteFilePath()},
        {QStringLiteral("projectPath"), projectInfo.absoluteFilePath()},
        {QStringLiteral("projectName"), projectInfo.completeBaseName()},
        {QStringLiteral("sourceMediaPath"), m_project.sourceMediaPath},
        {QStringLiteral("sourceName"), QFileInfo(m_project.sourceMediaPath).fileName()},
        {QStringLiteral("sourceLanguage"), m_project.sourceLanguage},
        {QStringLiteral("targetLanguage"), m_project.targetLanguage},
        {QStringLiteral("segmentCount"), m_project.segments.size()},
        {QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))}
    });
    while (updated.size() > 30) updated.removeLast();
    QSaveFile file(historyPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument::fromVariant(updated).toJson());
        if (file.commit()) {
            m_history = std::move(updated);
            emit historyChanged();
        }
    }
}

bool DubbingController::deleteHistoryItem(const QString &id)
{
    if (id.isEmpty()) return false;
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history.at(i).toMap().value(QStringLiteral("id")).toString() != id) continue;
        QVariantList updated = m_history;
        updated.removeAt(i);
        QSaveFile file(historyPath());
        if (!file.open(QIODevice::WriteOnly)) return false;
        file.write(QJsonDocument::fromVariant(updated).toJson());
        if (!file.commit()) return false;
        m_history = std::move(updated);
        emit historyChanged();
        return true;
    }
    return false;
}

void DubbingController::clearHistory()
{
    m_history.clear();
    QFile::remove(historyPath());
    emit historyChanged();
}

void DubbingController::closeProject()
{
    m_project = DubbingProject();
    m_workflowNodeConfigurations.clear();
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(QStringLiteral("import"));
    m_runner->cancel();
    if (m_workflowRunner) m_workflowRunner->setJournal(nullptr);
    m_workflowJournal.reset();
    emit projectChanged();
    emit segmentsChanged();
}

bool DubbingController::importMedia(const QString &pathOrUrl)
{
    const QString input = pathOrUrl.trimmed();
    const QString localPath = PathUtils::urlToLocalPath(input).trimmed();
    const QFileInfo fileInfo(localPath);
    const QString path = fileInfo.absoluteFilePath();
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Import media requested: input=\"%1\", local=\"%2\", exists=%3, isFile=%4")
                     .arg(input, localPath)
                     .arg(fileInfo.exists() ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(fileInfo.isFile() ? QStringLiteral("true") : QStringLiteral("false")));
    if (localPath.isEmpty() || !fileInfo.exists() || !fileInfo.isFile()) {
        Logger::error(QStringLiteral("DubbingController"),
                      QStringLiteral("Media import rejected: resolved path does not point to a file: \"%1\"").arg(path));
        setError(QStringLiteral("Media file does not exist: %1").arg(path));
        return false;
    }
    if (m_project.projectPath.isEmpty() && !newProject()) return false;

    // Import is intentionally side-effect free: preview the selected media and
    // reset downstream artifacts. Normalization and source separation only run
    // after the user chooses automatic or step-by-step processing.
    m_project.sourceMediaPath = path;
    m_project.sourceHash.clear();
    m_project.masterAudioPath.clear();
    m_project.analysisAudioPath.clear();
    m_project.backgroundAudioPath.clear();
    m_project.sourceDurationMs = 0;
    m_project.sourceSampleRate = 0;
    m_project.sourceChannels = 0;
    const QString suffix = fileInfo.suffix().toLower();
    m_project.sourceIsVideo = suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
        || suffix == QStringLiteral("mov") || suffix == QStringLiteral("webm") || suffix == QStringLiteral("avi");
    m_project.segments.clear();
    m_stepOutputs.clear();
    m_lastCompletedStepId.clear();
    m_runner->setBackgroundAudioPath(QString());
    m_runner->setPreviewPath(QString());
    m_runner->setExportPath(QString());
    setWorkflowMode(QStringLiteral("idle"));
    setCurrentStep(QStringLiteral("ingest"));
    emit projectChanged();
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
    return true;
}

void DubbingController::onIngestFinished(bool success, const QVariantMap &manifest)
{
    if (!success) return; // Error is already handled and set on runner
    
    m_project.sourceMediaPath = manifest.value(QStringLiteral("sourcePath")).toString();
    m_project.sourceHash = manifest.value(QStringLiteral("sourceHash")).toString();
    m_project.masterAudioPath = manifest.value(QStringLiteral("masterAudioPath")).toString();
    m_project.analysisAudioPath = manifest.value(QStringLiteral("analysisAudioPath")).toString();
    m_project.backgroundAudioPath = manifest.value(QStringLiteral("backgroundAudioPath")).toString();
    m_runner->setBackgroundAudioPath(manifest.value(QStringLiteral("backgroundAudioPath")).toString());
    m_project.sourceDurationMs = manifest.value(QStringLiteral("sourceDurationMs")).toLongLong();
    m_project.sourceSampleRate = manifest.value(QStringLiteral("sourceSampleRate")).toInt();
    m_project.sourceChannels = manifest.value(QStringLiteral("sourceChannels")).toInt();
    m_project.sourceIsVideo = manifest.value(QStringLiteral("sourceIsVideo")).toBool();
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Media normalized successfully: source=%1, hash=%2, master=%3, analysis=%4")
                     .arg(m_project.sourceMediaPath, m_project.sourceHash,
                          m_project.masterAudioPath, m_project.analysisAudioPath));
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::transcribeSource()
{
    if (m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import media before starting transcription."));
        return;
    }
    const QString audioPath = !m_project.analysisAudioPath.isEmpty() ? m_project.analysisAudioPath
                                                                     : m_project.masterAudioPath;
    if (audioPath.isEmpty()) {
        setError(QStringLiteral("Normalize and separate the source audio before transcription."));
        return;
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Starting dubbing transcription language=%1 audio=%2")
                     .arg(m_project.sourceLanguage, audioPath));
    m_runner->startTranscription(m_project.sourceLanguage, audioPath);
}

void DubbingController::translateSource()
{
    int emptySourceCount = 0;
    for (const QVariant &value : std::as_const(m_project.segments)) {
        if (value.toMap().value(QStringLiteral("sourceText")).toString().trimmed().isEmpty())
            ++emptySourceCount;
    }
    const QVariantMap configured =
        m_workflowNodeConfigurations.value(QStringLiteral("translate")).toMap();
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Translation requested sourceLanguage=%1 targetLanguage=%2 segments=%3 emptySource=%4 family=%5 runtime=%6 runtimeVersion=%7")
                     .arg(m_project.sourceLanguage, m_project.targetLanguage)
                     .arg(m_project.segments.size())
                     .arg(emptySourceCount)
                     .arg(configured.value(QStringLiteral("familyId")).toString(),
                          configured.value(QStringLiteral("runtimeId")).toString(),
                          configured.value(QStringLiteral("runtimeVersion")).toString()));
    if (m_project.sourceMediaPath.isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation rejected: source media is empty."));
        setError(QStringLiteral("Import media before translating."));
        return;
    }
    if (m_project.segments.isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation rejected: transcript has no segments."));
        setError(QStringLiteral("Transcribe the source before translating."));
        return;
    }
    if (m_project.targetLanguage.trimmed().isEmpty()) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation rejected: target language is empty."));
        setError(QStringLiteral("Choose a target language before translating."));
        return;
    }
    if (emptySourceCount > 0) {
        Logger::warning(QStringLiteral("DubbingController"),
                        QStringLiteral("Translation request contains %1 segment(s) without source text.")
                            .arg(emptySourceCount));
    }
    QVariantMap translationConfig = configured;
    QVariantMap parameters = translationConfig.value(QStringLiteral("parameters")).toMap();
    QVariantMap effectiveDurationControl = m_project.durationControl;
    effectiveDurationControl.insert(
        QStringLiteral("autoRewrite"),
        m_project.dubbingQuality != QStringLiteral("fast")
            && m_project.durationControl.value(QStringLiteral("autoRewrite"), true).toBool());
    parameters.insert(QStringLiteral("durationControl"), effectiveDurationControl);
    translationConfig.insert(QStringLiteral("parameters"), parameters);
    m_runner->setTranslationFixConfiguration(translationFixConfiguration());
    m_runner->startTranslation(m_project.sourceLanguage, m_project.targetLanguage, m_project.segments,
                               translationConfig);
}

void DubbingController::generateAudio()
{
    const QVariantMap synthesisConfiguration = m_workflowNodeConfigurations
        .value(QStringLiteral("synthesize")).toMap();
    QVariantMap synthesisSettings = synthesisConfiguration
        .value(QStringLiteral("parameters")).toMap();
    synthesisSettings.insert(QStringLiteral("familyId"),
                             synthesisConfiguration.value(QStringLiteral("familyId")));
    synthesisSettings.insert(QStringLiteral("autoReferenceSourcePath"),
                             !m_project.analysisAudioPath.isEmpty()
                                 ? m_project.analysisAudioPath : m_project.masterAudioPath);
    if (!synthesisSettings.contains(QStringLiteral("lang")))
        synthesisSettings.insert(QStringLiteral("lang"), m_project.targetLanguage);
    m_runner->startAudioGeneration(m_project.segments, m_project.projectPath, synthesisSettings);
}

void DubbingController::cancelProcessing()
{
    const bool wasAutomatic = m_workflowMode == QStringLiteral("automatic")
        || m_automaticSetupActive;
    m_automaticSetupActive = false;
    m_automaticOutputPath.clear();
    m_automaticDownloadsQueued.clear();
    m_automaticConfiguredNodes.clear();
    m_pendingExportPath.clear();
    if (m_translationFix) m_translationFix->cancel();
    if (m_workflowRunner && m_workflowRunner->running()) m_workflowRunner->cancel();
    m_runner->cancel();
    if (wasAutomatic) {
        setWorkflowMode(QStringLiteral("idle"));
        setAutomaticStatus(QStringLiteral("Automatic generation stopped"));
        appendAutomaticEvent(QStringLiteral("Automatic generation stopped"),
                             QStringLiteral("stopped"), currentStepId());
    }
    emit processingChanged();
    emit workflowChanged();
}

bool DubbingController::fixTranslations(const QVariantMap &configuration)
{
    if (!m_translationFix || processing()) return false;
    clearError();
    return m_translationFix->start(
        m_project.sourceLanguage, m_project.targetLanguage,
        m_project.segments, configuration);
}

bool DubbingController::fixTranslationSegment(
    int index, const QVariantMap &configuration)
{
    if (!m_translationFix || processing()
        || index < 0 || index >= m_project.segments.size())
        return false;
    clearError();
    return m_translationFix->start(
        m_project.sourceLanguage, m_project.targetLanguage,
        m_project.segments, configuration, index);
}

bool DubbingController::translationSegmentNeedsFix(int index) const
{
    if (index < 0 || index >= m_project.segments.size()) return false;
    return DubbingTranslationFixService::eligibleSegmentCount(
               {m_project.segments.at(index)}, m_project.targetLanguage)
        == 1;
}

void DubbingController::testTranslationFixConnection(
    const QVariantMap &configuration)
{
    if (!m_translationFix || processing()) return;
    m_translationFix->testConnection(configuration);
}

QVariantList DubbingController::translationFixCliModelOptions(
    const QString &cliAgent) const
{
    return DubbingTranslationFixService::cliModelOptions(cliAgent);
}

void DubbingController::cancelTranslationFix()
{
    if (m_translationFix) m_translationFix->cancel();
}

void DubbingController::setAdaptiveConfiguration(const QVariantMap &configuration)
{
    if (!m_translationFix || processing()) return;
    QVariantMap next = configuration;
    next.insert(QStringLiteral("configured"), true);
    m_translationFix->setConfiguration(next);
    if (m_project.dubbingQuality == QStringLiteral("custom"))
        m_project.customRewriteConfiguration = m_translationFix->configuration();
    if (m_runner) m_runner->setTranslationFixConfiguration(
        m_translationFix->configuration());
    persistAfterEdit();
    emit workflowChanged();
}

bool DubbingController::exportMedia(const QString &path)
{
    const QString outputPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (outputPath.isEmpty()) {
        setError(QStringLiteral("Choose an output path."));
        return false;
    }
    if (m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import source media before exporting."));
        return false;
    }
    if (previewPath().isEmpty() || !QFileInfo::exists(previewPath())) {
        m_pendingExportPath = outputPath;
        if (!renderPreview()) {
            m_pendingExportPath.clear();
            return false;
        }
        return true;
    }
    return m_runner->startExport(m_project.sourceMediaPath, previewPath(), outputPath,
                                 m_project.segments);
}

bool DubbingController::exportAudioStem(const QString &stem, const QString &path)
{
    const QString outputPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (outputPath.isEmpty()) {
        setError(QStringLiteral("Choose an audio output path."));
        return false;
    }

    QString sourcePath;
    if (stem == QStringLiteral("mix")) sourcePath = previewPath();
    else if (stem == QStringLiteral("vocal")) sourcePath = dubbedVocalPath();
    else if (stem == QStringLiteral("background")) sourcePath = m_project.backgroundAudioPath;
    else {
        setError(QStringLiteral("Unknown audio stem: %1").arg(stem));
        return false;
    }
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        setError(QStringLiteral("The %1 audio stem is not available yet.").arg(stem));
        return false;
    }
    QString error;
    if (!replaceCopy(sourcePath, outputPath, &error)) {
        setError(error);
        return false;
    }
    clearError();
    return true;
}

bool DubbingController::exportSubtitles(const QString &path, bool useTargetText)
{
    const QString outputPath = QFileInfo(PathUtils::urlToLocalPath(path)).absoluteFilePath();
    if (path.isEmpty() || outputPath.isEmpty()) {
        setError(QStringLiteral("Choose a subtitle output path."));
        return false;
    }
    QString error;
    if (!writeDubbingSubtitles(m_project.segments, outputPath, useTargetText, &error)) {
        setError(error);
        return false;
    }
    clearError();
    return true;
}

bool DubbingController::exportPackage(const QString &directoryPath)
{
    const QString outputDirectory = QFileInfo(PathUtils::urlToLocalPath(directoryPath)).absoluteFilePath();
    if (directoryPath.isEmpty() || outputDirectory.isEmpty()) {
        setError(QStringLiteral("Choose a package output folder."));
        return false;
    }
    if (!hasProject()) {
        setError(QStringLiteral("Save the dubbing project before exporting a package."));
        return false;
    }

    QDir directory(outputDirectory);
    if (!directory.mkpath(QStringLiteral(".")) || !directory.mkpath(QStringLiteral("clips"))) {
        setError(QStringLiteral("Cannot create package folder: %1").arg(outputDirectory));
        return false;
    }

    QString error;
    if (!m_project.save(&error)
        || !replaceCopy(m_project.projectPath, directory.filePath(QStringLiteral("project.ladub.json")), &error)
        || !replaceCopy(previewPath(), directory.filePath(QStringLiteral("dubbed-mix.wav")), &error)
        || !replaceCopy(dubbedVocalPath(), directory.filePath(QStringLiteral("dubbed-vocals.wav")), &error)
        || !replaceCopy(m_project.analysisAudioPath, directory.filePath(QStringLiteral("source-vocals.wav")), &error)
        || !replaceCopy(m_project.backgroundAudioPath, directory.filePath(QStringLiteral("background.wav")), &error)) {
        setError(error);
        return false;
    }

    int exportedClips = 0;
    for (int i = 0; i < m_project.segments.size(); ++i) {
        const QString clipPath = m_project.segments.at(i).toMap().value(QStringLiteral("clipPath")).toString();
        if (clipPath.isEmpty() || !QFileInfo(clipPath).isFile()) continue;
        const QString suffix = QFileInfo(clipPath).suffix().isEmpty()
            ? QStringLiteral("wav") : QFileInfo(clipPath).suffix();
        const QString clipName = QStringLiteral("%1.%2").arg(i + 1, 4, 10, QLatin1Char('0')).arg(suffix);
        if (!replaceCopy(clipPath, directory.filePath(QStringLiteral("clips/") + clipName), &error)) {
            setError(error);
            return false;
        }
        ++exportedClips;
    }

    const QString sourceSubtitlePath = directory.filePath(QStringLiteral("source.srt"));
    const QString dubbedSubtitlePath = directory.filePath(QStringLiteral("dubbed.srt"));
    if (!writeDubbingSubtitles(m_project.segments, sourceSubtitlePath, false, nullptr))
        QFile::remove(sourceSubtitlePath);
    if (!writeDubbingSubtitles(m_project.segments, dubbedSubtitlePath, true, nullptr))
        QFile::remove(dubbedSubtitlePath);

    const QJsonObject manifest{
        {QStringLiteral("format"), QStringLiteral("la-studio-dubbing-package")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("project"), QStringLiteral("project.ladub.json")},
        {QStringLiteral("sourceMediaPath"), m_project.sourceMediaPath},
        {QStringLiteral("segmentCount"), m_project.segments.size()},
        {QStringLiteral("exportedClipCount"), exportedClips}
    };
    QSaveFile manifestFile(directory.filePath(QStringLiteral("manifest.json")));
    if (!manifestFile.open(QIODevice::WriteOnly)
        || manifestFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) < 0
        || !manifestFile.commit()) {
        setError(QStringLiteral("Cannot write package manifest: %1").arg(manifestFile.errorString()));
        return false;
    }
    clearError();
    return true;
}

bool DubbingController::renderPreview(const QString &path)
{
    return m_runner->renderPreview(m_project.segments, m_project.projectPath, path);
}

void DubbingController::addSegment(qint64 startMs, qint64 endMs, const QString &sourceText)
{
    if (endMs <= startMs) {
        setError(QStringLiteral("Segment end must be after its start."));
        return;
    }
    QVariantMap segment;
    segment.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    segment.insert(QStringLiteral("startMs"), startMs);
    segment.insert(QStringLiteral("endMs"), endMs);
    segment.insert(QStringLiteral("sourceText"), sourceText);
    segment.insert(QStringLiteral("timingSource"), QStringLiteral("manual"));
    segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("pending"));
    segment.insert(QStringLiteral("targetText"), QString());
    segment.insert(QStringLiteral("speakerId"), QStringLiteral("speaker-1"));
    segment.insert(QStringLiteral("state"), QStringLiteral("draft"));
    m_project.segments.append(segment);
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::updateSegment(int index, const QVariantMap &patch)
{
    if (index < 0 || index >= m_project.segments.size()) return;
    QVariantMap segment = m_project.segments.at(index).toMap();
    const bool sourceTextChanged = patch.contains(QStringLiteral("sourceText"))
        && patch.value(QStringLiteral("sourceText")).toString()
            != segment.value(QStringLiteral("sourceText")).toString();
    const bool targetTextChanged = patch.contains(QStringLiteral("targetText"))
        && patch.value(QStringLiteral("targetText")).toString()
            != segment.value(QStringLiteral("targetText")).toString();
    const bool speakerChanged = patch.contains(QStringLiteral("speakerId"))
        && patch.value(QStringLiteral("speakerId")).toString()
            != segment.value(QStringLiteral("speakerId")).toString();
    const qint64 startMs = patch.value(QStringLiteral("startMs"), segment.value(QStringLiteral("startMs"))).toLongLong();
    const qint64 endMs = patch.value(QStringLiteral("endMs"), segment.value(QStringLiteral("endMs"))).toLongLong();
    if (endMs <= startMs) {
        setError(QStringLiteral("Segment end must be after its start."));
        return;
    }
    for (auto it = patch.cbegin(); it != patch.cend(); ++it) segment.insert(it.key(), it.value());
    segment.insert(QStringLiteral("startMs"), startMs);
    segment.insert(QStringLiteral("endMs"), endMs);
    if (sourceTextChanged) {
        // Word timestamps are derived from the source transcript. Any source edit
        // invalidates the previous alignment and forces the next refinement pass
        // to treat this segment as an ASR/manual-timing fallback.
        segment.remove(QStringLiteral("words"));
        segment.remove(QStringLiteral("alignmentCoverage"));
        segment.remove(QStringLiteral("alignmentMatchScore"));
        segment.remove(QStringLiteral("alignmentModel"));
        segment.remove(QStringLiteral("alignmentRuntime"));
        segment.insert(QStringLiteral("timingSource"), QStringLiteral("asr"));
        segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("pending"));
        segment.remove(QStringLiteral("durationBudget"));
        segment.remove(QStringLiteral("durationUnits"));
        segment.remove(QStringLiteral("durationStatus"));
        segment.remove(QStringLiteral("phonemeDistance"));
        segment.remove(QStringLiteral("referenceTranslation"));
        segment.remove(QStringLiteral("targetChunks"));
        segment.remove(QStringLiteral("pauseAligned"));
    }
    if (targetTextChanged || speakerChanged) {
        segment.insert(QStringLiteral("state"), QStringLiteral("stale"));
        if (targetTextChanged) {
            const QVariantMap budget = segment.value(QStringLiteral("durationBudget")).toMap();
            const int durationUnits = budget.isEmpty()
                ? -1
                : EspeakNgPhonemizer::count(
                      segment.value(QStringLiteral("targetText")).toString(),
                      m_project.targetLanguage);
            if (durationUnits >= 0) {
                const int minimum = budget.value(QStringLiteral("minUnits")).toInt();
                const int maximum = budget.value(QStringLiteral("maxUnits")).toInt();
                segment.insert(QStringLiteral("durationUnits"), durationUnits);
                segment.insert(QStringLiteral("phonemeDistance"),
                               qAbs(durationUnits
                                    - budget.value(QStringLiteral("targetUnits")).toInt()));
                segment.insert(QStringLiteral("durationStatus"),
                               durationUnits >= minimum && durationUnits <= maximum
                                   ? QStringLiteral("within-budget")
                                   : QStringLiteral("needs-review"));
            } else {
                segment.remove(QStringLiteral("durationUnits"));
                segment.remove(QStringLiteral("durationStatus"));
                segment.remove(QStringLiteral("phonemeDistance"));
            }
            segment.remove(QStringLiteral("durationPrompt"));
            segment.remove(QStringLiteral("targetChunks"));
            segment.remove(QStringLiteral("referenceTranslation"));
            segment.remove(QStringLiteral("pauseAligned"));
            segment.remove(QStringLiteral("candidateSelectionMetric"));
        }
    }
    m_project.segments[index] = segment;
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::removeSegment(int index)
{
    if (index < 0 || index >= m_project.segments.size()) return;
    m_project.segments.removeAt(index);
    emit segmentsChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::addSpeaker(const QString &name)
{
    QVariantMap speaker;
    speaker.insert(QStringLiteral("id"), QStringLiteral("speaker-%1").arg(m_project.speakers.size() + 1));
    speaker.insert(QStringLiteral("name"), name.trimmed().isEmpty()
                   ? QStringLiteral("Speaker %1").arg(m_project.speakers.size() + 1) : name.trimmed());
    speaker.insert(QStringLiteral("voice"), QVariantMap());
    m_project.speakers.append(speaker);
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::setSpeakerVoice(int speakerIndex, const QVariantMap &voice)
{
    if (speakerIndex < 0 || speakerIndex >= m_project.speakers.size()) return;
    QVariantMap speaker = m_project.speakers.at(speakerIndex).toMap();
    speaker.insert(QStringLiteral("voice"), voice);
    m_project.speakers[speakerIndex] = speaker;
    emit projectChanged();
    emit workflowChanged();
    persistAfterEdit();
}

void DubbingController::clearError()
{
    if (m_translationFix) m_translationFix->clearError();
    m_runner->clearError();
}

void DubbingController::setError(const QString &message)
{
    m_runner->setError(message);
}

void DubbingController::persistAfterEdit()
{
    if (!m_project.projectPath.isEmpty()) saveProject();
}

} // namespace LAStudio
