#include "DubbingJobRunner.h"
#include "DubbingProject.h"
#include "DubbingTranslationService.h"
#include "workflows/WorkflowArtifact.h"
#include "AppController.h"

#include "SttSessionController.h"
#include "tts/TtsEngine.h"
#include "audio/WavIO.h"
#include "MediaToolService.h"
#include "MediaIngestService.h"
#include "separation/SourceSeparationService.h"
#include "separation/SeparationTypes.h"
#include "AudioTimelineMixer.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include "core/ModelManager.h"
#include "core/RuntimeManager.h"

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QtMath>
#include <QUuid>
#include <QDirIterator>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent>

namespace LAStudio {

namespace {
QString synthesisFingerprint(const QVariantMap &segment, const QString &ttsSignature)
{
    const QVariantMap effective{{QStringLiteral("targetText"), segment.value(QStringLiteral("targetText"))},
                                {QStringLiteral("startMs"), segment.value(QStringLiteral("startMs"))},
                                {QStringLiteral("endMs"), segment.value(QStringLiteral("endMs"))},
                                {QStringLiteral("speakerId"), segment.value(QStringLiteral("speakerId"))},
                                {QStringLiteral("ttsSignature"), ttsSignature}};
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(QJsonObject::fromVariantMap(effective)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}
}

DubbingJobRunner::DubbingJobRunner(SttSessionController *sttSession, TtsEngine *tts,
                                   ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : QObject(parent), m_sttSession(sttSession), m_tts(tts), m_models(models), m_runtimes(runtimes)
{
    m_translationWatcher = new QFutureWatcher<QVariantList>(this);
    connect(m_translationWatcher, &QFutureWatcher<QVariantList>::finished,
            this, &DubbingJobRunner::onTranslationFinished);
    m_mediaTools = new MediaToolService(this);
    connect(m_mediaTools, &MediaToolService::finished,
            this, &DubbingJobRunner::onMediaFinished);
    m_mediaIngest = new MediaIngestService(this);
    connect(m_mediaIngest, &MediaIngestService::finished,
            this, &DubbingJobRunner::onIngestFinished);
    connect(m_mediaIngest, &MediaIngestService::progress, this, [this](int percent) {
        if (m_processing && m_stage == QStringLiteral("import")) {
            m_progress = percent;
            emit stateChanged();
        }
    });
    m_sourceSeparation = new SourceSeparationService(this);
    connect(m_sourceSeparation, &SourceSeparationService::finished, this, &DubbingJobRunner::onSourceSeparationFinished);

    if (m_sttSession) {
        connect(m_sttSession, &SttSessionController::transcriptionFinished,
                this, &DubbingJobRunner::onTranscriptionFinished);
        connect(m_sttSession, &SttSessionController::inputErrorChanged, this, [this]() {
            if (m_processing && !m_sttSession->inputError().isEmpty()) {
                setError(m_sttSession->inputError());
            }
        });
    }
    if (m_tts) {
        connect(m_tts, &TtsEngine::synthesisFinished, this, &DubbingJobRunner::onSynthesisFinished);
        connect(m_tts, &TtsEngine::errorOccurred, this, &DubbingJobRunner::onTtsError);
    }
}

DubbingJobRunner::~DubbingJobRunner()
{
    cancel();
}

void DubbingJobRunner::startIngest(const QString &path)
{
    if (m_processing) {
        setBusyError(QStringLiteral("A dubbing operation is already running."));
        return;
    }
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setProcessing(true, QStringLiteral("import"), 0);
    m_mediaIngest->ingest(path);
}

void DubbingJobRunner::startTranscription(const QString &sourceLanguage, const QString &sourceMediaPath)
{
    if (m_processing || (m_sttSession && m_sttSession->processing())) {
        setBusyError(QStringLiteral("Speech transcription is already running."));
        return;
    }
    if (!m_sttSession || sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import media before starting transcription."));
        return;
    }
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setProcessing(true, QStringLiteral("transcription"), 0);
    m_sttSession->setLanguage(sourceLanguage);
    m_sttSession->setTranslate(false);
    m_sttSession->selectFileInput(sourceMediaPath);
    m_sttSession->transcribeInput();
}

void DubbingJobRunner::startTranslation(const QString &sourceLanguage, const QString &targetLanguage, const QVariantList &segments)
{
    if (m_processing || m_translationWatcher->isRunning()) {
        setBusyError(QStringLiteral("A translation request is already running."));
        return;
    }
    DubbingTranslationService translationService(m_models, m_runtimes);
    DubbingTranslationRequest request;
    QString preparationError;
    if (!translationService.prepare(sourceLanguage, targetLanguage, request, &preparationError)) {
        setError(preparationError);
        return;
    }

    const quint64 generation = ++m_translationGeneration;
    m_translationInputSegments = segments;
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setProcessing(true, QStringLiteral("translation"), 0);
    m_translationWatcher->setFuture(QtConcurrent::run([segments, request, generation]() {
        QVariantList result;
        result.append(QVariantMap{{QStringLiteral("__workflowGeneration"), QString::number(generation)}});
        QString error;
        QVariantList patches;
        if (!DubbingTranslationService::translate(request, segments, patches, &error))
            result.append(QVariantMap{{QStringLiteral("error"), error}});
        else
            result.append(patches);
        return result;
    }));
}

void DubbingJobRunner::startAudioGeneration(const QVariantList &segments, const QString &projectPath)
{
    if (m_processing || (m_tts && m_tts->isProcessing())) {
        setBusyError(QStringLiteral("Speech synthesis is already running."));
        return;
    }
    if (!m_tts || !m_tts->isModelLoaded()) {
        setError(QStringLiteral("Load a TTS model before generating dubbing audio."));
        return;
    }
    m_activeSegments = segments;
    m_projectPath = projectPath;
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_generationIndex = -1;
    for (int i = 0; i < m_activeSegments.size(); ++i) {
        const QVariantMap segment = m_activeSegments.at(i).toMap();
        const QString fingerprint = synthesisFingerprint(segment, m_tts->activeSignature());
        const bool cacheValid = segment.value(QStringLiteral("state")).toString() == QStringLiteral("ready")
            && QFileInfo::exists(segment.value(QStringLiteral("clipPath")).toString())
            && segment.value(QStringLiteral("cacheFingerprint")).toString() == fingerprint;
        if (!segment.value(QStringLiteral("targetText")).toString().trimmed().isEmpty()
            && !cacheValid) {
            m_generationIndex = i;
            break;
        }
    }
    if (m_generationIndex < 0) {
        setError(QStringLiteral("Add target text to at least one segment before generating."));
        return;
    }
    setProcessing(true, QStringLiteral("tts"), 0);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_tts->synthesize(m_activeSegments.at(m_generationIndex).toMap().value(QStringLiteral("targetText")).toString());
}

void DubbingJobRunner::cancel()
{
    if (!m_processing) {
        ++m_translationGeneration;
        return;
    }
    if (m_stage == QStringLiteral("tts") && m_tts && m_tts->isProcessing()) m_tts->cancelProcessing();
    if (m_stage == QStringLiteral("transcription") && m_sttSession && m_sttSession->processing()) m_sttSession->cancelProcessing();
    if (m_stage == QStringLiteral("export") && m_mediaTools) m_mediaTools->cancel();
    if (m_stage == QStringLiteral("import") && m_mediaIngest) m_mediaIngest->cancel();
    if (m_sourceSeparation) m_sourceSeparation->cancel();
    if (m_stage == QStringLiteral("translation") && m_translationWatcher->isRunning()) m_translationWatcher->cancel();
    if (m_stage == QStringLiteral("export")) {
        QFile::remove(m_exportStagingPath);
        m_exportDestination.clear();
    }
    ++m_translationGeneration;
    m_generationIndex = -1;
    setProcessing(false, QStringLiteral("cancelled"), m_progress);
}

bool DubbingJobRunner::renderPreview(const QVariantList &segments, const QString &projectPath, const QString &path)
{
    if (m_processing) {
        setBusyError(QStringLiteral("Finish the active dubbing operation before rendering a preview."));
        return false;
    }
    QString outputPath = path;
    if (outputPath.isEmpty()) {
        if (projectPath.isEmpty()) {
            setError(QStringLiteral("Save the project before rendering a preview."));
            return false;
        }
        outputPath = QFileInfo(projectPath).absolutePath() + QStringLiteral("/preview.wav");
    }

    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QString error;
    if (!AudioTimelineMixer::mixSegments(segments, outputPath, m_backgroundAudioPath, &error)) {
        setError(error);
        return false;
    }

    m_previewPath = outputPath;
    emit stateChanged();
    emit stageCompleted(QStringLiteral("mix"), {{QStringLiteral("audio"), outputPath}});
    return true;
}

bool DubbingJobRunner::startExport(const QString &sourceMediaPath, const QString &outputPath)
{
    if (m_processing) {
        setBusyError(QStringLiteral("Finish the active dubbing operation before exporting."));
        return false;
    }
    if (outputPath.isEmpty()) {
        setError(QStringLiteral("Choose an output path."));
        return false;
    }
    if (sourceMediaPath.isEmpty()) {
        setError(QStringLiteral("Import source media before exporting."));
        return false;
    }
    if (m_previewPath.isEmpty() || !QFileInfo::exists(m_previewPath)) {
        setError(QStringLiteral("Generate and render audio preview before exporting."));
        return false;
    }
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_exportDestination = outputPath;
    const QString sourceSuffix = QFileInfo(sourceMediaPath).suffix().toLower();
    const bool video = sourceSuffix == QStringLiteral("mp4") || sourceSuffix == QStringLiteral("mkv")
        || sourceSuffix == QStringLiteral("mov") || sourceSuffix == QStringLiteral("webm")
        || sourceSuffix == QStringLiteral("avi");
    const QFileInfo destinationInfo(outputPath);
    const QString stagingSuffix = destinationInfo.suffix().isEmpty()
        ? QStringLiteral(".staging") : QStringLiteral(".") + destinationInfo.suffix();
    m_exportStagingPath = outputPath + QStringLiteral(".workflow-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + stagingSuffix;
    if (!video) {
        const QString stagingPath = m_exportStagingPath;
        if (!QFile::copy(m_previewPath, m_exportStagingPath)) {
            m_exportDestination.clear();
            m_exportStagingPath.clear();
            setError(QStringLiteral("Failed to stage rendered WAV for export: %1").arg(stagingPath));
            return false;
        }
        if (QFileInfo::exists(outputPath) && !QFile::remove(outputPath)) {
            QFile::remove(m_exportStagingPath);
            m_exportDestination.clear();
            m_exportStagingPath.clear();
            setError(QStringLiteral("Cannot replace existing export: %1").arg(outputPath));
            return false;
        }
        if (!QFile::rename(m_exportStagingPath, outputPath)) {
            QFile::remove(m_exportStagingPath);
            m_exportDestination.clear();
            m_exportStagingPath.clear();
            setError(QStringLiteral("Failed to commit export: %1").arg(outputPath));
            return false;
        }
        m_exportPath = outputPath;
        m_exportDestination.clear();
        m_exportStagingPath.clear();
        emit stateChanged();
        emit stageCompleted(QStringLiteral("export"), {{QStringLiteral("media"), outputPath}});
        return true;
    }

    if (!m_mediaTools) {
        m_exportDestination.clear();
        m_exportStagingPath.clear();
        setError(QStringLiteral("Media tool service is unavailable."));
        return false;
    }
    m_exportPath.clear();
    emit stateChanged();
    setProcessing(true, QStringLiteral("export"), 0);
    m_mediaTools->muxVideoWithAudio(sourceMediaPath, m_previewPath, m_exportStagingPath);
    return true;
}

void DubbingJobRunner::setPreviewPath(const QString &path)
{
    m_previewPath = path;
    emit stateChanged();
}

void DubbingJobRunner::setExportPath(const QString &path)
{
    m_exportPath = path;
    emit stateChanged();
}

void DubbingJobRunner::clearError()
{
    if (m_lastError.isEmpty()) return;
    m_lastError.clear();
    emit stateChanged();
}

void DubbingJobRunner::setProcessingState(bool value, const QString &stageValue, int progressValue)
{
    setProcessing(value, stageValue, progressValue);
}

void DubbingJobRunner::onTranscriptionFinished(const QString &text, const QVariantList &segments)
{
    Q_UNUSED(text);
    if (!m_processing || m_stage != QStringLiteral("transcription")) return;
    
    QVariantList updatedSegments;
    for (const QVariant &entry : segments) {
        const QVariantMap source = entry.toMap();
        QVariantMap segment;
        segment.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
        segment.insert(QStringLiteral("startMs"), qRound64(source.value(QStringLiteral("start")).toDouble() * 1000.0));
        segment.insert(QStringLiteral("endMs"), qRound64(source.value(QStringLiteral("end")).toDouble() * 1000.0));
        segment.insert(QStringLiteral("sourceText"), source.value(QStringLiteral("text")).toString().trimmed());
        segment.insert(QStringLiteral("targetText"), QString());
        segment.insert(QStringLiteral("speakerId"), QStringLiteral("speaker-1"));
        segment.insert(QStringLiteral("state"), QStringLiteral("transcribed"));
        updatedSegments.append(segment);
    }
    setProcessing(false, QStringLiteral("transcribed"), 100);
    emit segmentsUpdated(updatedSegments);
    emit stageCompleted(QStringLiteral("transcribe"), {{QStringLiteral("transcript"), updatedSegments}});
}

void DubbingJobRunner::onTranslationFinished()
{
    if (m_stage != QStringLiteral("translation")) return;
    const QVariantList rawResult = m_translationWatcher->result();
    if (rawResult.isEmpty()) {
        setError(QStringLiteral("Translation returned no result."));
        return;
    }
    const QVariantMap marker = rawResult.constFirst().toMap();
    if (marker.value(QStringLiteral("__workflowGeneration")).toString() != QString::number(m_translationGeneration)) return;
    QVariantList result = rawResult;
    result.removeFirst();
    if (!result.isEmpty() && result.constFirst().toMap().contains(QStringLiteral("error"))) {
        setError(result.constFirst().toMap().value(QStringLiteral("error")).toString());
        return;
    }
    QVariantList merged;
    QString mergeError;
    if (!DubbingProject::mergeSegmentPatches(m_translationInputSegments, result, merged, &mergeError)) {
        setError(mergeError);
        return;
    }
    setProcessing(false, QStringLiteral("translated"), 100);
    emit segmentsUpdated(merged);
    emit stageCompleted(QStringLiteral("translate"), {{QStringLiteral("transcript"), merged}});
}

void DubbingJobRunner::onSynthesisFinished(const QByteArray &pcm16, int sampleRate)
{
    Q_UNUSED(pcm16);
    if (!m_processing || m_generationIndex < 0 || m_generationIndex >= m_activeSegments.size()) return;
    const QVector<float> samples = m_tts ? m_tts->lastSamples() : QVector<float>();
    if (samples.isEmpty() || sampleRate <= 0) {
        setError(QStringLiteral("TTS returned empty audio for segment %1.").arg(m_generationIndex + 1));
        return;
    }

    const QVariantMap segment = m_activeSegments.at(m_generationIndex).toMap();
    const qint64 slotMs = qMax<qint64>(1, segment.value(QStringLiteral("endMs")).toLongLong()
                                          - segment.value(QStringLiteral("startMs")).toLongLong());
    const qint64 sourceDurationMs = qMax<qint64>(1, qRound64(samples.size() * 1000.0 / sampleRate));
    const double fitFactor = static_cast<double>(sourceDurationMs) / slotMs;
    const bool timingConflict = fitFactor < 0.85 || fitFactor > 1.20;
    const int fittedCount = qMax(1, qRound64(slotMs * sampleRate / 1000.0));
    const QVector<float> fittedSamples = timingConflict ? samples : AudioTimelineMixer::resampleToCount(samples, fittedCount);
    WorkflowArtifactStore artifactStore(QDir(QFileInfo(m_projectPath).absolutePath()).filePath(QStringLiteral(".workflow-artifacts")));
    QString artifactError;
    const QString stagingDir = artifactStore.createStagingDirectory(m_runId, m_activeNodeRunId, &artifactError);
    if (stagingDir.isEmpty()) {
        setError(artifactError);
        return;
    }
    const QString stagedClipPath = QDir(stagingDir).filePath(QStringLiteral("clip.wav"));
    if (!WavIO::saveFloat(stagedClipPath, fittedSamples.constData(), fittedSamples.size(), sampleRate)) {
        setError(QStringLiteral("Failed to stage generated clip: %1").arg(stagedClipPath));
        return;
    }
    WorkflowArtifactReference clipArtifact;
    if (!artifactStore.commitFile(stagedClipPath, QStringLiteral("audio.clip@1"), m_runId,
                                  m_activeNodeRunId, clipArtifact, &artifactError)) {
        setError(artifactError);
        return;
    }
    const QString clipPath = artifactStore.resolve(clipArtifact);
    if (clipPath.isEmpty()) {
        setError(QStringLiteral("Committed clip artifact cannot be resolved."));
        return;
    }

    QVariantMap updated = segment;
    updated.insert(QStringLiteral("clipPath"), clipPath);
    updated.insert(QStringLiteral("clipArtifact"), clipArtifact.toJson().toVariantMap());
    updated.insert(QStringLiteral("cacheFingerprint"), synthesisFingerprint(segment, m_tts->activeSignature()));
    updated.insert(QStringLiteral("sampleRate"), sampleRate);
    updated.insert(QStringLiteral("sampleCount"), fittedSamples.size());
    updated.insert(QStringLiteral("sourceDurationMs"), sourceDurationMs);
    updated.insert(QStringLiteral("durationMs"), qRound64(fittedSamples.size() * 1000.0 / sampleRate));
    updated.insert(QStringLiteral("fitFactor"), fitFactor);
    updated.insert(QStringLiteral("timingConflict"), timingConflict);
    updated.insert(QStringLiteral("fitMethod"), timingConflict ? QStringLiteral("none") : QStringLiteral("linear-resample"));
    updated.insert(QStringLiteral("state"), timingConflict ? QStringLiteral("conflict") : QStringLiteral("ready"));
    
    m_activeSegments[m_generationIndex] = updated;
    emit segmentUpdated(m_generationIndex, updated);

    int next = m_generationIndex + 1;
    while (next < m_activeSegments.size() && m_activeSegments.at(next).toMap().value(QStringLiteral("targetText")).toString().trimmed().isEmpty()) ++next;
    if (next >= m_activeSegments.size()) {
        m_generationIndex = -1;
        setProcessing(false, QStringLiteral("ready"), 100);
        emit stageCompleted(QStringLiteral("synthesize"), {{QStringLiteral("timeline"), m_activeSegments}});
        return;
    }
    m_generationIndex = next;
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setProcessing(true, QStringLiteral("tts"), qRound((100.0 * next) / m_activeSegments.size()));
    m_tts->synthesize(m_activeSegments.at(next).toMap().value(QStringLiteral("targetText")).toString());
}

void DubbingJobRunner::onTtsError(const QString &message)
{
    if (m_processing && m_generationIndex >= 0) setError(message);
}

void DubbingJobRunner::onMediaFinished(bool success, const QString &outputPath, const QString &error)
{
    if (!m_processing || m_stage != QStringLiteral("export")) return;
    if (!success) {
        QFile::remove(m_exportStagingPath);
        m_exportDestination.clear();
        m_exportStagingPath.clear();
        setError(error.isEmpty() ? QStringLiteral("Media export failed.") : error);
        return;
    }
    if (outputPath != m_exportStagingPath || !QFileInfo::exists(m_exportStagingPath)) {
        QFile::remove(m_exportStagingPath);
        m_exportDestination.clear();
        m_exportStagingPath.clear();
        setError(QStringLiteral("Media export completed without a valid staging artifact."));
        return;
    }
    if (QFileInfo::exists(m_exportDestination) && !QFile::remove(m_exportDestination)) {
        QFile::remove(m_exportStagingPath);
        setError(QStringLiteral("Cannot replace existing export: %1").arg(m_exportDestination));
        m_exportDestination.clear();
        m_exportStagingPath.clear();
        return;
    }
    if (!QFile::rename(m_exportStagingPath, m_exportDestination)) {
        QFile::remove(m_exportStagingPath);
        setError(QStringLiteral("Failed to commit export: %1").arg(m_exportDestination));
        m_exportDestination.clear();
        m_exportStagingPath.clear();
        return;
    }
    m_exportPath = m_exportDestination;
    m_exportDestination.clear();
    m_exportStagingPath.clear();
    setProcessing(false, QStringLiteral("exported"), 100);
    emit stageCompleted(QStringLiteral("export"), {{QStringLiteral("media"), outputPath}});
}

void DubbingJobRunner::onIngestFinished(bool success, const QVariantMap &manifest, const QString &error)
{
    if (m_stage != QStringLiteral("import")) return;
    if (!success) {
        setError(error.isEmpty() ? QStringLiteral("Media import failed.") : error);
        emit ingestFinished(false, {});
        return;
    }
    QString runtimePath = qEnvironmentVariable("SHERPA_ONNX_RUNTIME");
    QString modelPath = qEnvironmentVariable("SHERPA_ONNX_UVR_MODEL");
    AppController *app = AppController::instance();
    if (runtimePath.isEmpty() || !QFileInfo(runtimePath).isFile()) {
        if (app && app->runtimes()) {
            for (const auto &rtVal : app->runtimes()->allRuntimes()) {
                QString rtId = rtVal.toMap().value(QStringLiteral("id")).toString();
                if (rtId == QStringLiteral("sherpa-onnx-win-x86_64-cpu") ||
                    rtId == QStringLiteral("sherpa-onnx-source-separation-win-x86_64-cpu")) {
                    QString path = app->runtimes()->getRuntimePath(rtId);
                    if (!path.isEmpty() && QFileInfo(path).isFile()) {
                        runtimePath = path;
                        break;
                    }
                }
            }
        }
    }
    if (modelPath.isEmpty() || !QFileInfo(modelPath).isFile()) {
        if (app && app->models()) {
            for (const QString &modelId : {QStringLiteral("k2-fsa/sherpa-onnx-uvr-vocals-ft"),
                                          QStringLiteral("k2-fsa/sherpa-onnx-source-separation")}) {
                const QString path = app->models()->filePath(modelId, QStringLiteral("UVR-MDX-NET-Voc_FT.onnx"));
                if (!path.isEmpty() && QFileInfo(path).isFile()) {
                    modelPath = path;
                    break;
                }
            }
        }
    }
    const QString inputAudio = manifest.value(QStringLiteral("masterAudioPath")).toString();
    if (!runtimePath.isEmpty() && !modelPath.isEmpty() && QFileInfo(runtimePath).isFile() && QFileInfo(modelPath).isFile() && !inputAudio.isEmpty()) {
        m_pendingIngestManifest = manifest;
        setProcessing(true, QStringLiteral("source-separation"), 70);
        
        SeparationConfiguration config;
        config.backendId = QStringLiteral("sherpa-onnx");
        config.pipelineProfile = QStringLiteral("uvr-2stems");
        config.runtimeId = QStringLiteral("sherpa-onnx-win-x86_64-cpu");
        config.runtimeVersion = QStringLiteral("v1.13.4");
        config.runtimePath = runtimePath;
        config.familyId = QStringLiteral("k2-fsa/sherpa-onnx-uvr-vocals-ft");
        config.configurationSignature = QStringLiteral("uvr-vocals-ft");
        config.modelFilesByRole.insert(QStringLiteral("model"), modelPath);
        
        SeparationRequest req;
        req.sourcePath = inputAudio;
        req.outputRoot = PathUtils::cacheDir() + QStringLiteral("/dubbing/source-separation/");
        req.configuration = config;
        req.numThreads = 4;
        
        QString err;
        if (!m_sourceSeparation->isolate(req, &err)) {
            setProcessing(false, QStringLiteral("imported"), 100);
            QVariantMap failedManifest = manifest;
            failedManifest.insert(QStringLiteral("sourceSeparationWarning"), err);
            emit ingestFinished(true, failedManifest);
            emit stageCompleted(QStringLiteral("ingest"), failedManifest);
        }
        return;
    }
    setProcessing(false, QStringLiteral("imported"), 100);
    emit ingestFinished(true, manifest);
    emit stageCompleted(QStringLiteral("ingest"), manifest);
}

void DubbingJobRunner::onSourceSeparationFinished(const SeparationResult &result)
{
    if (m_stage != QStringLiteral("source-separation")) return;
    QVariantMap manifest = m_pendingIngestManifest;
    m_pendingIngestManifest.clear();
    if (result.success) {
        QString vocalsPath;
        QString bgPath;
        for (const auto &stem : result.stems) {
            if (stem.id == QStringLiteral("vocals")) {
                vocalsPath = stem.path;
            } else if (stem.id == QStringLiteral("background")) {
                bgPath = stem.path;
            }
        }
        manifest.insert(QStringLiteral("analysisAudioPath"), vocalsPath);
        manifest.insert(QStringLiteral("backgroundAudioPath"), bgPath);
        manifest.insert(QStringLiteral("sourceSeparation"), QStringLiteral("uvr"));
    } else {
        manifest.insert(QStringLiteral("sourceSeparationWarning"), result.error.isEmpty() ? QStringLiteral("Source separation failed; using original audio.") : result.error);
    }
    setProcessing(false, QStringLiteral("imported"), 100);
    emit ingestFinished(true, manifest);
    emit stageCompleted(QStringLiteral("ingest"), manifest);
}

void DubbingJobRunner::setError(const QString &message)
{
    m_lastError = message;
    setProcessing(false, QStringLiteral("error"), 0);
    emit errorOccurred(message);
}

void DubbingJobRunner::setBusyError(const QString &message)
{
    m_lastError = message;
    emit stateChanged();
    emit errorOccurred(message);
}

void DubbingJobRunner::setProcessing(bool value, const QString &stageValue, int progressValue)
{
    m_processing = value;
    m_stage = stageValue;
    m_progress = qBound(0, progressValue, 100);
    emit stateChanged();
}

} // namespace LAStudio
