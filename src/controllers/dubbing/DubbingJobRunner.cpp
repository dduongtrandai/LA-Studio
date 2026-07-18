#include "controllers/dubbing/DubbingJobRunner.h"
#include "controllers/models/StudioConfigurationResolver.h"
#include "translation/TranslationService.h"
#include "dubbing/DubbingProject.h"
#include "dubbing/DubbingTranslationService.h"
#include "translation/TranslationEngine.h"
#include "dubbing/AlignmentRefinementService.h"
#include "dubbing/DubbingSegmentNormalizer.h"
#include "workflows/WorkflowArtifact.h"
#include "controllers/app/AppController.h"

#include "SttSessionController.h"
#include "tts/TtsEngine.h"
#include "audio/WavIO.h"
#include "dubbing/media/MediaToolService.h"
#include "dubbing/media/MediaIngestService.h"
#include "separation/SourceSeparationService.h"
#include "separation/SeparationTypes.h"
#include "dubbing/AudioTimelineMixer.h"
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

DubbingJobRunner::DubbingJobRunner(SttSessionController *sttSession, TtsEngine *tts,
                                   ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : DubbingJobRunner(sttSession, tts, nullptr, models, runtimes, parent)
{
}

namespace {
QVariantList buildWaveformPreview(const QVector<float> &samples, int maximumPoints = 360)
{
    QVariantList preview;
    if (samples.isEmpty() || maximumPoints <= 0) return preview;

    const int pointCount = qMin(samples.size(), maximumPoints);
    preview.reserve(pointCount);
    for (int point = 0; point < pointCount; ++point) {
        const int begin = point * samples.size() / pointCount;
        const int end = qMax(begin + 1, (point + 1) * samples.size() / pointCount);
        float peak = 0.0f;
        for (int sample = begin; sample < end; ++sample)
            peak = qMax(peak, qAbs(samples.at(sample)));
        preview.append(peak);
    }
    return preview;
}

QString synthesisFingerprint(const QVariantMap &segment, const QString &ttsSignature,
                             const QVariantMap &synthesisSettings)
{
    const QVariantMap effective{{QStringLiteral("targetText"), segment.value(QStringLiteral("targetText"))},
                                {QStringLiteral("startMs"), segment.value(QStringLiteral("startMs"))},
                                {QStringLiteral("endMs"), segment.value(QStringLiteral("endMs"))},
                                {QStringLiteral("speakerId"), segment.value(QStringLiteral("speakerId"))},
                                {QStringLiteral("ttsSignature"), ttsSignature},
                                {QStringLiteral("synthesisSettings"), synthesisSettings}};
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(QJsonObject::fromVariantMap(effective)).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}
}

DubbingJobRunner::DubbingJobRunner(SttSessionController *sttSession, TtsEngine *tts,
                                   TranslationEngine *translation,
                                   ModelManager *models, RuntimeManager *runtimes, QObject *parent)
    : QObject(parent), m_sttSession(sttSession), m_tts(tts), m_translation(translation), m_models(models), m_runtimes(runtimes)
{
    m_alignmentWatcher = new QFutureWatcher<QVariantMap>(this);
    connect(m_alignmentWatcher, &QFutureWatcher<QVariantMap>::finished,
            this, &DubbingJobRunner::onAlignmentFinished);
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
        connect(m_sttSession, &SttSessionController::transcriptionFailed, this,
                [this](const QString &message) {
            if (!m_processing || m_stage != QStringLiteral("transcription")) return;
            m_waitingForTranscriptionInput = false;
            setError(message);
        });
        connect(m_sttSession, &SttSessionController::progressChanged, this, [this]() {
            if (!m_processing || m_stage != QStringLiteral("transcription")
                || m_waitingForTranscriptionInput) return;
            const int progress = qBound(5, m_sttSession->progress(), 99);
            if (progress == m_progress) return;
            m_progress = progress;
            emit stateChanged();
        });
        connect(m_sttSession, &SttSessionController::inputErrorChanged, this, [this]() {
            if (m_processing && !m_sttSession->inputError().isEmpty()) {
                Logger::error(QStringLiteral("DubbingPipeline"),
                              QStringLiteral("STT input decode failed: %1").arg(m_sttSession->inputError()));
                m_waitingForTranscriptionInput = false;
                setError(m_sttSession->inputError());
            }
        });
        connect(m_sttSession, &SttSessionController::inputLoadingChanged, this, [this]() {
            if (!m_processing || m_stage != QStringLiteral("transcription")
                || !m_waitingForTranscriptionInput || m_sttSession->inputLoading()) return;
            if (!m_sttSession->inputError().isEmpty()) return;

            m_waitingForTranscriptionInput = false;
            Logger::info(QStringLiteral("DubbingPipeline"),
                         QStringLiteral("STT input decoded; starting inference for %1").arg(m_sttSession->inputPath()));
            m_progress = 5;
            emit stateChanged();
            m_sttSession->transcribeInput();
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
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[ingest] start run=%1 node=%2 input=%3")
                     .arg(m_runId, m_activeNodeRunId, path));
    setProcessing(true, QStringLiteral("import"), 0);
    m_mediaIngest->ingest(path);
}

void DubbingJobRunner::startSourceSeparation(const QString &audioPath,
                                             const QVariantMap &modelConfiguration)
{
    if (m_processing) {
        setBusyError(QStringLiteral("A dubbing operation is already running."));
        return;
    }
    if (audioPath.isEmpty() || !QFileInfo::exists(audioPath)) {
        setError(QStringLiteral("Normalize the source media before separating speech."));
        return;
    }

    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[source-separate] requested audio=%1 size=%2 bytes")
                     .arg(audioPath).arg(QFileInfo(audioPath).size()));

    SeparationConfiguration config;
    QString runtimePath;
    QString modelPath;
    AppController *app = AppController::instance();

    if (!modelConfiguration.value(QStringLiteral("familyId")).toString().isEmpty()) {
        StudioConfiguration selection;
        selection.capabilityId = QStringLiteral("voice-isolation");
        selection.familyId = modelConfiguration.value(QStringLiteral("familyId")).toString();
        selection.runtimeId = modelConfiguration.value(QStringLiteral("runtimeId")).toString();
        selection.runtimeVersion = modelConfiguration.value(QStringLiteral("runtimeVersion")).toString();
        selection.selectedFiles = modelConfiguration.value(QStringLiteral("selectedFiles")).toMap();

        const ResolvedConfiguration resolved = StudioConfigurationResolver::resolve(selection);
        if (!resolved.isValid || !QFileInfo(resolved.runtimePath).isFile()) {
            setError(QStringLiteral("The selected voice isolation model or runtime is unavailable."));
            return;
        }

        config.backendId = resolved.resolvedPaths.value(
            QStringLiteral("backend"), QStringLiteral("sherpa-onnx")).toString();
        config.pipelineProfile = resolved.resolvedPaths.value(QStringLiteral("pipelineProfile")).toString();
        config.runtimeId = selection.runtimeId;
        config.runtimeVersion = selection.runtimeVersion;
        config.runtimePath = resolved.runtimePath;
        config.familyId = selection.familyId;
        config.configurationSignature = resolved.signature;

        const QVariantList requiredFiles = resolved.family.value(QStringLiteral("requiredFiles")).toList();
        for (const QVariant &requiredValue : requiredFiles) {
            const QString role = requiredValue.toMap().value(QStringLiteral("role")).toString();
            const QString path = resolved.resolvedPaths.value(role).toString();
            if (role.isEmpty() || path.isEmpty() || !QFileInfo(path).isFile()) {
                setError(QStringLiteral("A required voice isolation model file is unavailable: %1").arg(role));
                return;
            }
            config.modelFilesByRole.insert(role, path);
        }
    } else {
        runtimePath = qEnvironmentVariable("SHERPA_ONNX_RUNTIME");
        modelPath = qEnvironmentVariable("SHERPA_ONNX_UVR_MODEL");
        if (runtimePath.isEmpty() || !QFileInfo(runtimePath).isFile()) {
            if (app && app->runtimes()) {
                for (const auto &rtVal : app->runtimes()->allRuntimes()) {
                    const QString rtId = rtVal.toMap().value(QStringLiteral("id")).toString();
                    if (rtId != QStringLiteral("sherpa-onnx-win-x86_64-cpu")
                        && rtId != QStringLiteral("sherpa-onnx-source-separation-win-x86_64-cpu")) continue;
                    const QString path = app->runtimes()->getRuntimePath(rtId);
                    if (!path.isEmpty() && QFileInfo(path).isFile()) { runtimePath = path; break; }
                }
            }
        }
        if (modelPath.isEmpty() || !QFileInfo(modelPath).isFile()) {
            if (app && app->models()) {
                for (const QString &modelId : {QStringLiteral("k2-fsa/sherpa-onnx-uvr-vocals-ft"),
                                              QStringLiteral("k2-fsa/sherpa-onnx-source-separation")}) {
                    const QString path = app->models()->filePath(modelId, QStringLiteral("UVR-MDX-NET-Voc_FT.onnx"));
                    if (!path.isEmpty() && QFileInfo(path).isFile()) { modelPath = path; break; }
                }
            }
        }

        if (runtimePath.isEmpty() || modelPath.isEmpty()
            || !QFileInfo(runtimePath).isFile() || !QFileInfo(modelPath).isFile()) {
            const QVariantMap outputs{{QStringLiteral("vocals"), audioPath},
                                      {QStringLiteral("background"), audioPath},
                                      {QStringLiteral("warning"), QStringLiteral("Voice isolation runtime or model is unavailable; using normalized audio.")}};
            Logger::warning(QStringLiteral("DubbingPipeline"),
                            QStringLiteral("[source-separate] runtime/model unavailable; using normalized audio"));
            emit sourceSeparationFinished(outputs);
            emit stageCompleted(QStringLiteral("source-separate"), outputs);
            return;
        }

        config.backendId = QStringLiteral("sherpa-onnx");
        config.pipelineProfile = QStringLiteral("uvr-2stems");
        config.runtimeId = QStringLiteral("sherpa-onnx-win-x86_64-cpu");
        config.runtimeVersion = QStringLiteral("v1.13.4");
        config.runtimePath = runtimePath;
        config.familyId = QStringLiteral("k2-fsa/sherpa-onnx-uvr-vocals-ft");
        config.configurationSignature = QStringLiteral("uvr-vocals-ft");
        config.modelFilesByRole.insert(QStringLiteral("model"), modelPath);
    }

    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingSourceAudioPath = audioPath;
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[source-separate] runtime=%1 family=%2")
                     .arg(config.runtimePath, config.familyId));
    setProcessing(true, QStringLiteral("source-separation"), 0);

    SeparationRequest request;
    request.sourcePath = audioPath;
    request.outputRoot = PathUtils::cacheDir() + QStringLiteral("/dubbing/source-separation/");
    request.configuration = config;
    request.numThreads = 4;
    QString error;
    if (!m_sourceSeparation->isolate(request, &error)) {
        m_pendingSourceAudioPath.clear();
        setError(error);
        Logger::error(QStringLiteral("DubbingPipeline"),
                      QStringLiteral("[source-separate] failed to start: %1").arg(error));
    }
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
    if (!m_sttSession->canTranscribe()) {
        setError(QStringLiteral("The STT model is not ready. Wait for model loading to finish and try again."));
        return;
    }
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[transcription] start run=%1 node=%2 language=%3 audio=%4 size=%5 bytes")
                     .arg(m_runId, m_activeNodeRunId, sourceLanguage, sourceMediaPath)
                     .arg(QFileInfo(sourceMediaPath).size()));
    setProcessing(true, QStringLiteral("transcription"), 0);
    m_sttSession->setLanguage(sourceLanguage);
    m_sttSession->setTranslate(false);
    m_sttSession->selectFileInput(sourceMediaPath);
    m_transcriptionAudioPath = sourceMediaPath;
    m_transcriptionLanguage = sourceLanguage;
    m_waitingForTranscriptionInput = true;
    Logger::debug(QStringLiteral("DubbingPipeline"),
                  QStringLiteral("[transcription] waiting for asynchronous audio decode"));
    if (!m_sttSession->inputLoading() && m_sttSession->inputError().isEmpty()) {
        m_waitingForTranscriptionInput = false;
        Logger::info(QStringLiteral("DubbingPipeline"), QStringLiteral("[transcription] audio was already decoded; starting inference"));
        m_progress = 5;
        emit stateChanged();
        m_sttSession->transcribeInput();
    }
}

void DubbingJobRunner::startTranslation(const QString &sourceLanguage, const QString &targetLanguage, const QVariantList &segments,
                                        const QVariantMap &modelConfiguration)
{
    if (m_processing || (m_translationInstance && m_translationInstance->isProcessing())) {
        setBusyError(QStringLiteral("A translation request is already running."));
        return;
    }
    if (!m_translation) {
        setError(QStringLiteral("Translation engine is unavailable."));
        return;
    }
    DubbingTranslationService translationService(m_models, m_runtimes);
    DubbingTranslationRequest request;
    QString preparationError;
    bool prepared = false;
    if (!modelConfiguration.isEmpty()) {
        StudioConfiguration selected;
        selected.capabilityId = QStringLiteral("translation");
        selected.familyId = modelConfiguration.value(QStringLiteral("familyId")).toString();
        selected.runtimeId = modelConfiguration.value(QStringLiteral("runtimeId")).toString();
        selected.runtimeVersion = modelConfiguration.value(QStringLiteral("runtimeVersion")).toString();
        selected.selectedFiles = modelConfiguration.value(QStringLiteral("selectedFiles")).toMap();
        const ResolvedConfiguration resolved = StudioConfigurationResolver::resolve(selected);
        if (resolved.isValid) {
            SessionConfiguration sessionConfig;
            sessionConfig.capabilityId = selected.capabilityId;
            sessionConfig.selection.capabilityId = selected.capabilityId;
            sessionConfig.selection.familyId = selected.familyId;
            sessionConfig.selection.runtimeId = selected.runtimeId;
            sessionConfig.selection.runtimeVersion = selected.runtimeVersion;
            sessionConfig.familyConfig = resolved.family;
            sessionConfig.runtimePath = resolved.runtimePath;
            for (auto it = resolved.resolvedPaths.cbegin(); it != resolved.resolvedPaths.cend(); ++it) {
                if (it.key() == QStringLiteral("familyId") || it.key() == QStringLiteral("backend") || it.key() == QStringLiteral("pipelineProfile")) continue;
                sessionConfig.resolvedPathsByRole.insert(it.key(), it.value().toString());
                if (it.value().toString().endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) sessionConfig.resolvedModelPaths.append(it.value().toString());
            }
            TranslationService::prepareConfiguration(sessionConfig, sourceLanguage, targetLanguage, request, &preparationError);
            prepared = !request.modelPath.isEmpty();
        }
    }
    if (!prepared) prepared = translationService.prepare(sourceLanguage, targetLanguage, request, &preparationError);
    if (!prepared) {
        setError(preparationError);
        return;
    }

    const quint64 generation = ++m_translationGeneration;
    m_translationInputSegments = segments;
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[translation] start run=%1 node=%2 source=%3 target=%4 segments=%5")
                     .arg(m_runId, m_activeNodeRunId, sourceLanguage, targetLanguage).arg(segments.size()));
    setProcessing(true, QStringLiteral("translation"), 0);

    SessionConfiguration configuration;
    configuration.capabilityId = QStringLiteral("translation");
    configuration.selection.capabilityId = configuration.capabilityId;
    configuration.selection.familyId = request.backend;
    configuration.selection.runtimeId = request.useGpu ? QStringLiteral("translation-cuda") : QStringLiteral("translation-cpu");
    configuration.familyConfig = QVariantMap{{QStringLiteral("id"), request.backend},
                                             {QStringLiteral("backend"), request.backend}};
    configuration.runtimePath = request.runtimePath;
    configuration.resolvedModelPaths = {request.modelPath};
    configuration.resolvedPathsByRole.insert(QStringLiteral("model"), request.modelPath);
    configuration.signature = QStringLiteral("dubbing|%1|%2|%3")
        .arg(request.backend, request.modelPath, request.runtimePath);

    m_translationInstance = m_translation->instance(configuration.signature);
    if (!m_translationInstance) {
        m_translationInstance = m_translation->loadInstance(configuration, false);
    }
    if (!m_translationInstance) {
        setError(QStringLiteral("Failed to create Translation instance."));
        return;
    }

    TranslationInferenceRequest inferenceRequest;
    inferenceRequest.segments = segments;
    inferenceRequest.sourceLanguage = sourceLanguage;
    inferenceRequest.targetLanguage = targetLanguage;
    inferenceRequest.maxTokens = request.maxTokens;
    inferenceRequest.cancellation = InferenceCancellationToken();
    m_translationResult.clear();

    QObject::disconnect(m_translationFinishedConnection);
    QObject::disconnect(m_translationErrorConnection);
    QObject::disconnect(m_translationLoadedConnection);
    m_translationFinishedConnection = connect(m_translationInstance, &TranslationEngineInstance::translationFinished,
        this, [this, generation](const QVariantList &patches) {
            if (generation != m_translationGeneration || m_stage != QStringLiteral("translation")) return;
            m_translationResult = {QVariantMap{{QStringLiteral("__workflowGeneration"), QString::number(generation)}}};
            for (const QVariant &patch : patches) m_translationResult.append(patch);
            onTranslationFinished();
        });
    m_translationErrorConnection = connect(m_translationInstance, &TranslationEngineInstance::errorOccurred,
        this, [this, generation](const QString &error) {
            if (generation == m_translationGeneration && m_stage == QStringLiteral("translation")) setError(error);
        });
    auto startWhenReady = [this, inferenceRequest, generation]() {
        if (!m_translationInstance || generation != m_translationGeneration) return;
        if (m_translationInstance->state() == TranslationEngineInstance::Ready) {
            m_translationInstance->translate(inferenceRequest);
        } else if (m_translationInstance->state() == TranslationEngineInstance::Error) {
            setError(QStringLiteral("Translation model failed to load."));
        }
    };
    if (m_translationInstance->isModelLoaded()) {
        startWhenReady();
    } else {
        m_translationLoadedConnection = connect(m_translationInstance, &TranslationEngineInstance::modelLoadedChanged,
                                                this, startWhenReady);
    }
}

void DubbingJobRunner::startAudioGeneration(const QVariantList &segments, const QString &projectPath,
                                            const QVariantMap &synthesisSettings)
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
    // Snapshot the node settings once per run. Every segment in this run must
    // use the same preset even if the inspector changes while synthesis runs.
    m_synthesisSettings = synthesisSettings;
    if (m_runId.isEmpty()) m_runId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_generationIndex = -1;
    for (int i = 0; i < m_activeSegments.size(); ++i) {
        const QVariantMap segment = m_activeSegments.at(i).toMap();
        const QString fingerprint = synthesisFingerprint(
            segment, m_tts->activeSignature(), m_synthesisSettings);
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
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[tts] start run=%1 segments=%2 firstIndex=%3 model=%4 voice=%5 project=%6")
                     .arg(m_runId).arg(m_activeSegments.size()).arg(m_generationIndex)
                     .arg(m_tts->activeSignature(),
                          m_synthesisSettings.value(QStringLiteral("voice")).toString(),
                          projectPath));
    setProcessing(true, QStringLiteral("tts"), 0);
    m_activeNodeRunId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_tts->synthesize(
        m_activeSegments.at(m_generationIndex).toMap().value(QStringLiteral("targetText")).toString(),
        0, 1.0f, m_synthesisSettings);
}

void DubbingJobRunner::cancel()
{
    if (!m_processing) {
        ++m_translationGeneration;
        return;
    }
    if (m_stage == QStringLiteral("tts") && m_tts && m_tts->isProcessing()) m_tts->cancelProcessing();
    if (m_stage == QStringLiteral("transcription") && m_sttSession && m_sttSession->processing()) m_sttSession->cancelProcessing();
    if (m_stage == QStringLiteral("alignment") && m_alignmentCancel) {
        m_alignmentCancel->storeRelease(true);
        m_alignmentCancel.reset();
    }
    if (m_stage == QStringLiteral("export") && m_mediaTools) m_mediaTools->cancel();
    if (m_stage == QStringLiteral("import") && m_mediaIngest) m_mediaIngest->cancel();
    if (m_sourceSeparation) m_sourceSeparation->cancel();
    if (m_stage == QStringLiteral("translation") && m_translationInstance && m_translationInstance->isProcessing()) {
        m_translationInstance->cancelProcessing();
    }
    Logger::warning(QStringLiteral("DubbingPipeline"),
                    QStringLiteral("[%1] cancelled at %2%%").arg(m_stage).arg(m_progress));
    m_waitingForTranscriptionInput = false;
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
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[mix] start run=%1 node=%2 segments=%3 background=%4 output=%5")
                     .arg(m_runId).arg(m_activeNodeRunId).arg(segments.size())
                     .arg(m_backgroundAudioPath).arg(outputPath));

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
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[export] start run=%1 node=%2 source=%3 preview=%4 output=%5")
                     .arg(m_runId, m_activeNodeRunId, sourceMediaPath, m_previewPath, outputPath));

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

    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[transcription] finished segments=%1 textChars=%2 elapsedMs=%3")
                     .arg(segments.size()).arg(text.size()).arg(m_stageTimer.isValid() ? m_stageTimer.elapsed() : -1));
    
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
        segment.insert(QStringLiteral("timingSource"), QStringLiteral("asr"));
        segment.insert(QStringLiteral("alignmentStatus"), QStringLiteral("pending"));
        segment.insert(QStringLiteral("state"), QStringLiteral("transcribed"));
        updatedSegments.append(segment);
    }
    startAlignmentRefinement(m_transcriptionAudioPath, m_transcriptionLanguage, updatedSegments);
}

void DubbingJobRunner::startAlignmentRefinement(const QString &audioPath, const QString &language,
                                                const QVariantList &segments)
{
    if (!m_alignmentWatcher) {
        const QVariantList normalized = DubbingSegmentNormalizer::normalize(segments);
        setProcessing(false, QStringLiteral("transcribed"), 100);
        emit segmentsUpdated(normalized);
        emit stageCompleted(QStringLiteral("transcribe"), {{QStringLiteral("transcript"), normalized}});
        return;
    }

    // Keep the language explicit in the refinement cache key and allow a
    // deployment-level override for older workflow callers.
    const QString effectiveLanguage = qEnvironmentVariable("LASTUDIO_DUBBING_LANGUAGE",
                                                           language.isEmpty() ? QStringLiteral("en") : language);
    const QString preset = qEnvironmentVariable("LASTUDIO_DUBBING_ALIGNMENT_PRESET",
                                                QStringLiteral("balanced"));
    const QVariantList input = segments;
    const QString path = audioPath;
    m_alignmentCancel = std::make_shared<QAtomicInteger<bool>>(false);
    const auto cancel = m_alignmentCancel;
    setProcessing(true, QStringLiteral("alignment"), 10);
    m_alignmentWatcher->setFuture(QtConcurrent::run([this, path, effectiveLanguage, input, preset, cancel]() {
        const AlignmentRefinementResult refined = AlignmentRefinementService::refine(
            path, effectiveLanguage, input, m_models, m_runtimes, preset, cancel.get());
        return QVariantMap{{QStringLiteral("segments"), refined.segments},
                           {QStringLiteral("status"), refined.status},
                           {QStringLiteral("diagnostic"), refined.diagnostic},
                           {QStringLiteral("attempted"), refined.attempted},
                           {QStringLiteral("changed"), refined.changed}};
    }));
}

void DubbingJobRunner::onAlignmentFinished()
{
    if (!m_alignmentWatcher || m_stage != QStringLiteral("alignment")) return;
    const QVariantMap result = m_alignmentWatcher->result();
    const QVariantList alignedSegments = result.value(QStringLiteral("segments")).toList();
    if (alignedSegments.isEmpty()) {
        setError(QStringLiteral("Forced alignment returned no transcript segments."));
        return;
    }
    const QVariantList segments = DubbingSegmentNormalizer::normalize(alignedSegments);
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[alignment] status=%1 changed=%2 segments=%3->%4 diagnostic=%5")
                     .arg(result.value(QStringLiteral("status")).toString())
                     .arg(result.value(QStringLiteral("changed")).toBool())
                     .arg(alignedSegments.size())
                     .arg(segments.size())
                     .arg(result.value(QStringLiteral("diagnostic")).toString()));
    setProcessing(false, QStringLiteral("transcribed"), 100);
    emit segmentsUpdated(segments);
    emit stageCompleted(QStringLiteral("transcribe"), {{QStringLiteral("transcript"), segments}});
    m_alignmentCancel.reset();
}

void DubbingJobRunner::onTranslationFinished()
{
    if (m_stage != QStringLiteral("translation")) return;
    const QVariantList rawResult = m_translationResult;
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[translation] worker finished resultItems=%1 elapsedMs=%2")
                     .arg(rawResult.size()).arg(m_stageTimer.isValid() ? m_stageTimer.elapsed() : -1));
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
    Logger::debug(QStringLiteral("DubbingPipeline"),
                  QStringLiteral("[tts] segment finished index=%1 pcmBytes=%2 sampleRate=%3")
                      .arg(m_generationIndex).arg(pcm16.size()).arg(sampleRate));
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
    updated.insert(QStringLiteral("cacheFingerprint"),
                   synthesisFingerprint(segment, m_tts->activeSignature(), m_synthesisSettings));
    updated.insert(QStringLiteral("sampleRate"), sampleRate);
    updated.insert(QStringLiteral("sampleCount"), fittedSamples.size());
    updated.insert(QStringLiteral("waveformSamples"), buildWaveformPreview(fittedSamples));
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
    // TtsEngineInstance emits synthesisFinished before returning to Ready.
    // Queue the next request so the Processing state does not discard it.
    QMetaObject::invokeMethod(this, [this, next]() {
        if (!m_processing || m_stage != QStringLiteral("tts") || m_generationIndex != next || !m_tts) return;
        m_tts->synthesize(
            m_activeSegments.at(next).toMap().value(QStringLiteral("targetText")).toString(),
            0, 1.0f, m_synthesisSettings);
    }, Qt::QueuedConnection);
}

void DubbingJobRunner::onTtsError(const QString &message)
{
    if (m_processing && m_generationIndex >= 0) setError(message);
}

void DubbingJobRunner::onMediaFinished(bool success, const QString &outputPath, const QString &error)
{
    if (!m_processing || m_stage != QStringLiteral("export")) return;
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[export] media tool finished success=%1 output=%2 elapsedMs=%3")
                     .arg(success ? QStringLiteral("true") : QStringLiteral("false"), outputPath)
                     .arg(m_stageTimer.isValid() ? m_stageTimer.elapsed() : -1));
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
    emit stageCompleted(QStringLiteral("export"), {{QStringLiteral("media"), m_exportPath}});
}

void DubbingJobRunner::onIngestFinished(bool success, const QVariantMap &manifest, const QString &error)
{
    if (m_stage != QStringLiteral("import")) return;
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[ingest] finished success=%1 manifestKeys=%2 elapsedMs=%3")
                     .arg(success ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(manifest.keys().join(QLatin1Char(',')))
                     .arg(m_stageTimer.isValid() ? m_stageTimer.elapsed() : -1));
    if (!success) {
        setError(error.isEmpty() ? QStringLiteral("Media import failed.") : error);
        emit ingestFinished(false, {});
        return;
    }
    setProcessing(false, QStringLiteral("imported"), 100);
    emit ingestFinished(true, manifest);
    emit stageCompleted(QStringLiteral("ingest"), manifest);
}

void DubbingJobRunner::onSourceSeparationFinished(const SeparationResult &result)
{
    if (m_stage != QStringLiteral("source-separation")) return;
    Logger::info(QStringLiteral("DubbingPipeline"),
                 QStringLiteral("[source-separate] finished success=%1 stems=%2 elapsedMs=%3 error=%4")
                     .arg(result.success ? QStringLiteral("true") : QStringLiteral("false"))
                     .arg(result.stems.size())
                     .arg(m_stageTimer.isValid() ? m_stageTimer.elapsed() : -1)
                     .arg(result.error));
    QVariantMap outputs;
    const QString fallbackAudio = m_pendingSourceAudioPath;
    m_pendingSourceAudioPath.clear();
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
        outputs.insert(QStringLiteral("vocals"), vocalsPath.isEmpty() ? fallbackAudio : vocalsPath);
        outputs.insert(QStringLiteral("background"), bgPath.isEmpty() ? fallbackAudio : bgPath);
        outputs.insert(QStringLiteral("sourceSeparation"), QStringLiteral("uvr"));
    } else {
        outputs.insert(QStringLiteral("vocals"), fallbackAudio);
        outputs.insert(QStringLiteral("background"), fallbackAudio);
        outputs.insert(QStringLiteral("warning"), result.error.isEmpty() ? QStringLiteral("Source separation failed; using normalized audio.") : result.error);
    }
    setProcessing(false, QStringLiteral("separated"), 100);
    emit sourceSeparationFinished(outputs);
    emit stageCompleted(QStringLiteral("source-separate"), outputs);
}

void DubbingJobRunner::setError(const QString &message)
{
    Logger::error(QStringLiteral("DubbingPipeline"),
                  QStringLiteral("[%1] error at %2%%: %3").arg(m_stage).arg(m_progress).arg(message));
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
    if (value && (!m_processing || m_stage != stageValue)) {
        m_stageTimer.start();
        Logger::debug(QStringLiteral("DubbingPipeline"),
                      QStringLiteral("stage entered: %1 progress=%2").arg(stageValue).arg(progressValue));
    } else if (!value && m_processing) {
        Logger::debug(QStringLiteral("DubbingPipeline"),
                      QStringLiteral("stage leaving: %1 progress=%2 elapsedMs=%3")
                          .arg(m_stage).arg(progressValue).arg(m_stageTimer.isValid() ? m_stageTimer.elapsed() : -1));
    }
    m_processing = value;
    m_stage = stageValue;
    m_progress = qBound(0, progressValue, 100);
    emit stateChanged();
}

} // namespace LAStudio
