#include "controllers/dubbing/DubbingController.h"

#include "SttSessionController.h"
#include "tts/TtsEngine.h"
#include "controllers/dubbing/DubbingJobRunner.h"
#include "controllers/dubbing/DubbingTranslationFixService.h"
#include "dubbing/workflow/DubbingWorkflowDefinition.h"
#include "dubbing/workflow/DubbingWorkflowNodes.h"
#include "workflows/WorkflowGraphRunner.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include "controllers/app/AppController.h"
#include "controllers/models/ModelSessionRegistry.h"
#include "controllers/models/StudioConfigurationResolver.h"
#include "controllers/models/CapabilitySettingsSchema.h"

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

namespace LAStudio {

namespace {

void unloadConflictingDubbingRuntime(ModelSessionRegistry *registry,
                                     const QString &capabilityId)
{
    if (!registry) return;

    QString conflictingCapability;
    if (capabilityId == QStringLiteral("tts")) {
        conflictingCapability = QStringLiteral("translation");
    } else if (capabilityId == QStringLiteral("translation")) {
        conflictingCapability = QStringLiteral("tts");
    } else {
        return;
    }

    IModelSession *conflictingSession =
        registry->sessionForCapability(conflictingCapability);
    if (!conflictingSession) return;

    const QList<SessionConfiguration> loaded =
        conflictingSession->loadedConfigurations();
    if (loaded.isEmpty()) return;

    Logger::info(
        QStringLiteral("DubbingController"),
        QStringLiteral("Dubbing runtime handoff: unloading %1 before loading %2 "
                       "to avoid incompatible shared DLLs in one process.")
            .arg(conflictingCapability, capabilityId));
    for (const SessionConfiguration &configuration : loaded) {
        conflictingSession->requestUnloadConfiguration(configuration.signature);
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

DubbingController::DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                                     TranslationEngine *translation,
                                     ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : QObject(parent), m_sttSession(sttSession), m_tts(tts)
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
            });
            connect(session, &IModelSession::activeConfigurationChanged, this, [this]() {
                emit workflowChanged();
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
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::failed, this, [this](const QString &) {
        if (m_workflowReviewStore && !m_activeReviewId.isEmpty()) m_workflowReviewStore->remove(m_activeReviewId);
        m_activeReviewId.clear();
        m_workflowReviewRequest.clear();
        setWorkflowMode(QStringLiteral("idle"));
        emit workflowChanged();
    });

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
                                       destination)) {
                emit workflowChanged();
                return;
            }
        }
        if (m_workflowMode == QStringLiteral("step")
            && (!m_workflowRunner || !m_workflowRunner->running())) {
            advanceManualStep(nodeId);
        }
        emit workflowChanged();
    });
}

bool DubbingController::processing() const
{
    return (m_translationFix && m_translationFix->busy())
        || m_runner->processing()
        || (m_workflowRunner && m_workflowRunner->running());
}

QString DubbingController::stage() const
{
    if (m_translationFix && m_translationFix->busy())
        return QStringLiteral("translation-fix");
    return m_runner->stage();
}

int DubbingController::progress() const
{
    if (m_translationFix && m_translationFix->busy())
        return m_translationFix->progress();
    return m_workflowRunner && m_workflowRunner->running() ? m_workflowRunner->progress() : m_runner->progress();
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

QString DubbingController::previewPath() const
{
    return m_runner->previewPath();
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
            detail = ttsReady ? QStringLiteral("TTS model loaded") : QStringLiteral("Load a TTS model");
            provider = ttsReady ? QStringLiteral("Local TTS") : QStringLiteral("No model loaded");
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
        const QVariantMap selected = m_workflowNodeConfigurations.value(definition.id).toMap();
        if (!selected.isEmpty()) {
            item.insert(QStringLiteral("providerName"), selected.value(QStringLiteral("modelName")));
            item.insert(QStringLiteral("selectedFamilyId"), selected.value(QStringLiteral("familyId")));
            item.insert(QStringLiteral("selectedRuntimeId"), selected.value(QStringLiteral("runtimeId")));
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
    const bool translationReady = !translationConfigured || translatedArtifactReady
        || (AppController::instance() && AppController::instance()->sessionRegistry()
            && AppController::instance()->sessionRegistry()->sessionForCapability(QStringLiteral("translation"))
            && AppController::instance()->sessionRegistry()->sessionForCapability(QStringLiteral("translation"))->canProcess());
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

    QVariantMap selected{{QStringLiteral("familyId"), familyId},
                         {QStringLiteral("runtimeId"), config.runtimeId},
                         {QStringLiteral("runtimeVersion"), config.runtimeVersion},
                          {QStringLiteral("selectedFiles"), config.selectedFiles},
                          {QStringLiteral("modelName"), family.value(QStringLiteral("title"))},
                          {QStringLiteral("capabilityId"), capabilityId},
                          {QStringLiteral("configurationSignature"), resolved.signature},
                          {QStringLiteral("familyConfig"), family},
                          {QStringLiteral("parameterDefinitions"), family.value(QStringLiteral("parameterDefinitions"))},
                          {QStringLiteral("studioConfig"),
                           family.value(QStringLiteral("studio")).toMap().value(capabilityId)}};
    m_workflowNodeConfigurations.insert(nodeId, selected);
    unloadConflictingDubbingRuntime(app->sessionRegistry(), capabilityId);
    if (IModelSession *session = app->sessionRegistry()->sessionForCapability(capabilityId)) {
        session->requestLoad(capabilityId, config);
    }
    Logger::info(QStringLiteral("DubbingController"),
                 QStringLiteral("Workflow node model changed node=%1 family=%2 runtime=%3")
                     .arg(nodeId, familyId, config.runtimeId));
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
    emit workflowChanged();
    return true;
}

QString DubbingController::workflowStatusText() const
{
    if (processing()) return QStringLiteral("Running %1 (%2%)").arg(stage()).arg(progress());
    if (workflowReady()) return QStringLiteral("Workflow configured and ready to run");
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
        const QString active = m_workflowRunner->activeNodeId();
        if (active == QStringLiteral("media-input")) return QStringLiteral("import");
        if (active == QStringLiteral("review-transcript")) return QStringLiteral("transcribe");
        if (active == QStringLiteral("review-translation")) return QStringLiteral("translate");
        if (active == QStringLiteral("assign-voices") || active == QStringLiteral("fit-timing")) return QStringLiteral("synthesize");
        if (active == QStringLiteral("review-conflicts")) return QStringLiteral("mix");
        return active;
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
    for (auto &node : graph.nodes) {
        if (node.id == QStringLiteral("translate"))
            node.parameters.insert(QStringLiteral("durationControl"), m_project.durationControl);
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
        } else if (node.id == QStringLiteral("translate")) {
            node.parameters.insert(QStringLiteral("sourceLanguage"), m_project.sourceLanguage);
            node.parameters.insert(QStringLiteral("targetLanguage"), m_project.targetLanguage);
            node.properties = node.parameters;
        } else if (node.typeId == QStringLiteral("core.review-gate")) {
            node.parameters.insert(QStringLiteral("mode"), QStringLiteral("never"));
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("synthesize")) {
            node.parameters.insert(QStringLiteral("projectPath"), m_project.projectPath);
            node.parameters.insert(QStringLiteral("synthesisSettings"),
                                   modelConfig.value(QStringLiteral("parameters")).toMap());
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
    setWorkflowMode(QStringLiteral("automatic"));
    setCurrentStep(QStringLiteral("ingest"));
    if (runWorkflow(outputPath)) return true;
    setWorkflowMode(QStringLiteral("idle"));
    return false;
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

    setWorkflowMode(QStringLiteral("step"));
    setCurrentStep(step);
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
    parameters.insert(QStringLiteral("durationControl"), m_project.durationControl);
    translationConfig.insert(QStringLiteral("parameters"), parameters);
    m_runner->startTranslation(m_project.sourceLanguage, m_project.targetLanguage, m_project.segments,
                               translationConfig);
}

void DubbingController::generateAudio()
{
    const QVariantMap synthesisSettings = m_workflowNodeConfigurations
        .value(QStringLiteral("synthesize")).toMap()
        .value(QStringLiteral("parameters")).toMap();
    m_runner->startAudioGeneration(m_project.segments, m_project.projectPath, synthesisSettings);
}

void DubbingController::cancelProcessing()
{
    m_pendingExportPath.clear();
    if (m_translationFix) m_translationFix->cancel();
    if (m_workflowRunner && m_workflowRunner->running()) m_workflowRunner->cancel();
    m_runner->cancel();
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

void DubbingController::cancelTranslationFix()
{
    if (m_translationFix) m_translationFix->cancel();
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
    return m_runner->startExport(m_project.sourceMediaPath, outputPath);
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
    const qint64 startMs = patch.value(QStringLiteral("startMs"), segment.value(QStringLiteral("startMs"))).toLongLong();
    const qint64 endMs = patch.value(QStringLiteral("endMs"), segment.value(QStringLiteral("endMs"))).toLongLong();
    if (endMs <= startMs) {
        setError(QStringLiteral("Segment end must be after its start."));
        return;
    }
    for (auto it = patch.cbegin(); it != patch.cend(); ++it) segment.insert(it.key(), it.value());
    segment.insert(QStringLiteral("startMs"), startMs);
    segment.insert(QStringLiteral("endMs"), endMs);
    if (patch.contains(QStringLiteral("sourceText"))) {
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
    if (patch.contains(QStringLiteral("targetText")) || patch.contains(QStringLiteral("speakerId"))) {
        segment.insert(QStringLiteral("state"), QStringLiteral("stale"));
        if (patch.contains(QStringLiteral("targetText"))) {
            segment.remove(QStringLiteral("durationUnits"));
            segment.remove(QStringLiteral("durationStatus"));
            segment.remove(QStringLiteral("durationPrompt"));
            segment.remove(QStringLiteral("targetChunks"));
            segment.remove(QStringLiteral("phonemeDistance"));
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
