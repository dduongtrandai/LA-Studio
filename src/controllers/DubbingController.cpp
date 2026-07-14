#include "DubbingController.h"

#include "SttSessionController.h"
#include "tts/TtsEngine.h"
#include "DubbingJobRunner.h"
#include "workflows/DubbingWorkflowDefinition.h"
#include "workflows/DubbingWorkflowNodes.h"
#include "workflows/WorkflowGraphRunner.h"
#include "core/PathUtils.h"
#include "core/Logger.h"

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtMath>
#include <QUrl>
#include <QUuid>
#include <QDir>
#include <QDateTime>

namespace LAStudio {

DubbingController::DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                                     ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : QObject(parent), m_sttSession(sttSession), m_tts(tts)
{
    m_runner = new DubbingJobRunner(sttSession, tts, models, runtimes, this);
    m_workflowRegistry = new NodeRegistry(this);
    registerDubbingWorkflowNodes(*m_workflowRegistry, m_runner);
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
        emit workflowChanged();
    });
    connect(m_workflowRunner, &WorkflowGraphRunner::failed, this, [this](const QString &) {
        if (m_workflowReviewStore && !m_activeReviewId.isEmpty()) m_workflowReviewStore->remove(m_activeReviewId);
        m_activeReviewId.clear();
        m_workflowReviewRequest.clear();
        emit workflowChanged();
    });

    connect(m_runner, &DubbingJobRunner::stateChanged, this, [this]() {
        emit processingChanged();
        emit errorChanged();
        emit previewChanged();
        emit exportChanged();
        emit workflowChanged();
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
}

bool DubbingController::processing() const
{
    return m_runner->processing() || (m_workflowRunner && m_workflowRunner->running());
}

QString DubbingController::stage() const
{
    return m_runner->stage();
}

int DubbingController::progress() const
{
    return m_workflowRunner && m_workflowRunner->running() ? m_workflowRunner->progress() : m_runner->progress();
}

QString DubbingController::lastError() const
{
    return (m_workflowRunner && !m_workflowRunner->error().isEmpty()) ? m_workflowRunner->error() : m_runner->lastError();
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
    bool hasClips = false;
    bool hasConflict = false;
    for (const QVariant &entry : m_project.segments) {
        const QVariantMap segment = entry.toMap();
        hasTargets = hasTargets || !segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty();
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
            state = hasMedia ? QStringLiteral("completed") : QStringLiteral("missing");
            detail = hasMedia ? QStringLiteral("Media normalized") : QStringLiteral("Import source media");
        } else if (definition.id == QStringLiteral("transcribe")) {
            state = hasSegments ? QStringLiteral("completed") : (hasMedia ? QStringLiteral("ready") : QStringLiteral("missing"));
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
            state = hasTargets ? QStringLiteral("ready") : QStringLiteral("blocked");
            detail = hasTargets ? QStringLiteral("Review speaker voice assignments") : QStringLiteral("Translated transcript required");
        } else if (definition.id == QStringLiteral("synthesize")) {
            state = !ttsReady ? QStringLiteral("missing") : (hasClips ? QStringLiteral("completed") : (hasTargets ? QStringLiteral("ready") : QStringLiteral("blocked")));
            detail = ttsReady ? QStringLiteral("TTS model loaded") : QStringLiteral("Load a TTS model");
            provider = ttsReady ? QStringLiteral("Local TTS") : QStringLiteral("No model loaded");
        } else if (definition.id == QStringLiteral("fit-timing")) {
            state = hasConflict ? QStringLiteral("blocked") : (hasClips ? QStringLiteral("completed") : QStringLiteral("ready"));
            detail = hasConflict ? QStringLiteral("One or more clips exceed the fit tolerance") : QStringLiteral("Fit generated clips to segment timing");
        } else if (definition.id == QStringLiteral("review-conflicts")) {
            state = hasConflict ? QStringLiteral("blocked") : (hasClips ? QStringLiteral("completed") : QStringLiteral("ready"));
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
        item.insert(QStringLiteral("typeId"), definition.typeId);
        item.insert(QStringLiteral("typeVersion"), definition.typeVersion);
        result.append(item);
    }
    return result;
}

bool DubbingController::workflowReady() const
{
    return workflowGraphValid()
        && !m_project.sourceMediaPath.isEmpty()
        && !m_project.segments.isEmpty()
        && !m_project.targetLanguage.trimmed().isEmpty()
        && m_tts && m_tts->isModelLoaded();
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
        if (node.id == QStringLiteral("media-input")) {
            node.parameters.insert(QStringLiteral("value"), m_project.sourceMediaPath);
            node.properties.insert(QStringLiteral("value"), m_project.sourceMediaPath);
        } else if (node.id == QStringLiteral("translate")) {
            node.parameters.insert(QStringLiteral("sourceLanguage"), m_project.sourceLanguage);
            node.parameters.insert(QStringLiteral("targetLanguage"), m_project.targetLanguage);
            node.properties = node.parameters;
        } else if (node.id == QStringLiteral("synthesize") || node.id == QStringLiteral("mix")) {
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
    return true;
}

void DubbingController::closeProject()
{
    m_project = DubbingProject();
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
    
    m_runner->startIngest(path);
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
    m_runner->startTranscription(m_project.sourceLanguage, m_project.sourceMediaPath);
}

void DubbingController::translateSource()
{
    if (m_project.sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import media before translating."));
        return;
    }
    if (m_project.segments.isEmpty()) {
        setError(QStringLiteral("Transcribe the source before translating."));
        return;
    }
    m_runner->startTranslation(m_project.sourceLanguage, m_project.targetLanguage, m_project.segments);
}

void DubbingController::generateAudio()
{
    m_runner->startAudioGeneration(m_project.segments, m_project.projectPath);
}

void DubbingController::cancelProcessing()
{
    if (m_workflowRunner && m_workflowRunner->running()) m_workflowRunner->cancel();
    m_runner->cancel();
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
        if (!renderPreview()) return false;
    }
    return m_runner->startExport(m_project.sourceMediaPath, outputPath);
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
    if (patch.contains(QStringLiteral("targetText")) || patch.contains(QStringLiteral("speakerId"))) {
        segment.insert(QStringLiteral("state"), QStringLiteral("stale"));
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
