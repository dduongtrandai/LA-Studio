#include "controllers/alignment/AlignmentExecutionService.h"

#include "core/Logger.h"
#include "core/ModelManager.h"
#include "core/RuntimeManager.h"
#include "controllers/alignment/AlignmentTranscriptMatcher.h"
#include "SttAudioDecoder.h"
#include "runtimes/CrispAlignmentInterface.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QDirIterator>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <algorithm>

namespace LAStudio {

namespace {
constexpr qsizetype kMaxStdoutBytes = 16 * 1024 * 1024;
constexpr qsizetype kMaxStderrBytes = 1024 * 1024;

QString findFile(const QString &rootOrFile, const QStringList &names)
{
    const QFileInfo direct(rootOrFile);
    if (direct.isFile()) return direct.absoluteFilePath();
    QDirIterator it(rootOrFile, names, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

QString formatTimestamp(double seconds, bool vtt)
{
    const qint64 ms = qMax<qint64>(0, qRound64(seconds * 1000.0));
    const int h = int(ms / 3600000), m = int((ms / 60000) % 60), s = int((ms / 1000) % 60);
    return QStringLiteral("%1:%2:%3%4%5").arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'))
        .arg(vtt ? QLatin1Char('.') : QLatin1Char(',')).arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

QVariantMap runAlignmentProcess(const QString &executable, const QString &modelDir,
                                const QString &audioPath, const QString &transcript,
                                const QString &language, const QString &timestampUnit,
                                const QString &outputFormat, bool normalizeTranscript)
{
    QVariantMap outcome;
    if (!QFileInfo(executable).isFile()) {
        outcome.insert(QStringLiteral("errorCode"), QStringLiteral("ALIGNMENT_RUNTIME_NOT_INSTALLED"));
        outcome.insert(QStringLiteral("error"), QStringLiteral("The selected MMS alignment process runtime is not installed."));
        return outcome;
    }

    const QJsonObject request{
        {QStringLiteral("operation"), QStringLiteral("align")},
        {QStringLiteral("audio"), QDir::fromNativeSeparators(audioPath)},
        {QStringLiteral("transcript"), transcript},
        {QStringLiteral("language"), language},
        {QStringLiteral("model_dir"), QDir::fromNativeSeparators(modelDir)},
        {QStringLiteral("timestamp_unit"), timestampUnit},
        {QStringLiteral("output_format"), outputFormat},
        {QStringLiteral("normalize_transcript"), normalizeTranscript}
    };

    QProcess process;
    process.setProgram(executable);
    process.setWorkingDirectory(QFileInfo(executable).absolutePath());
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(10000)) {
        outcome.insert(QStringLiteral("errorCode"), QStringLiteral("ALIGNMENT_RUNTIME_START_FAILED"));
        outcome.insert(QStringLiteral("error"), QStringLiteral("The MMS alignment runtime could not be started."));
        return outcome;
    }
    process.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
    process.write("\n");
    process.closeWriteChannel();
    if (!process.waitForFinished(300000)) {
        process.kill();
        process.waitForFinished(1000);
        outcome.insert(QStringLiteral("errorCode"), QStringLiteral("ALIGNMENT_RUNTIME_TIMEOUT"));
        outcome.insert(QStringLiteral("error"), QStringLiteral("The MMS alignment runtime timed out."));
        return outcome;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        outcome.insert(QStringLiteral("errorCode"), QStringLiteral("ALIGNMENT_RUNTIME_RESPONSE_INVALID"));
        outcome.insert(QStringLiteral("error"), QStringLiteral("The MMS alignment runtime returned an invalid response."));
        return outcome;
    }
    const QJsonObject response = document.object();
    if (!response.value(QStringLiteral("ok")).toBool()) {
        const QJsonObject error = response.value(QStringLiteral("error")).toObject();
        outcome.insert(QStringLiteral("errorCode"), error.value(QStringLiteral("code")).toString(
            QStringLiteral("ALIGNMENT_RUNTIME_FAILED")));
        outcome.insert(QStringLiteral("error"), error.value(QStringLiteral("message")).toString(
            QStringLiteral("The MMS alignment runtime failed.")));
        return outcome;
    }

    outcome.insert(QStringLiteral("segments"), response.value(QStringLiteral("segments")).toArray().toVariantList());
    outcome.insert(QStringLiteral("duration"), response.value(QStringLiteral("duration")).toDouble());
    outcome.insert(QStringLiteral("output"), response.value(QStringLiteral("output")).toString());
    outcome.insert(QStringLiteral("diagnostics"), QVariantList{});
    return outcome;
}
}

AlignmentExecutionService::AlignmentExecutionService(RuntimeManager *runtimes,
                                                     ModelManager *models,
                                                     QObject *parent)
    : QObject(parent)
    , m_runtimes(runtimes)
    , m_models(models)
    , m_workflowResolver(runtimes, models)
    , m_workflowSession(this)
{
    connect(&m_workflowSession, &WorkflowSession::changed, this, &AlignmentExecutionService::workflowChanged);
    if (m_models)
        connect(m_models, &ModelManager::registryUpdated, &m_workflowSession, &WorkflowSession::invalidate);
    if (m_runtimes)
        connect(m_runtimes, &RuntimeManager::registryUpdated, &m_workflowSession, &WorkflowSession::invalidate);
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        m_terminalProcessError = true;
        setError(QStringLiteral("RUNTIME_TIMEOUT"), QStringLiteral("Alignment runtime timed out."));
        m_process.kill();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        m_stdout.append(m_process.readAllStandardOutput());
        if (m_stdout.size() > kMaxStdoutBytes) {
            m_terminalProcessError = true;
            setError(QStringLiteral("RESPONSE_TOO_LARGE"), QStringLiteral("Alignment runtime returned too much data."));
            m_process.kill();
        }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_stderr.append(m_process.readAllStandardError());
        if (m_stderr.size() > kMaxStderrBytes) m_stderr = m_stderr.right(kMaxStderrBytes);
    });
    connect(&m_process, &QProcess::started, this, [this]() {
        m_statusText = QStringLiteral("Aligning");
        emit stateChanged();
    });
    connect(&m_process, &QProcess::finished, this, &AlignmentExecutionService::handleFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_terminalProcessError = true;
            setError(QStringLiteral("RUNTIME_START_FAILED"), QStringLiteral("Alignment runtime could not be started."));
        }
    });
}

AlignmentExecutionService::~AlignmentExecutionService()
{
    if (processing()) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QString AlignmentExecutionService::localPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    if (url.isLocalFile()) return QDir::toNativeSeparators(url.toLocalFile());
    return QDir::toNativeSeparators(pathOrUrl);
}

QVariantList AlignmentExecutionService::installedAnchorModels() const
{
    QVariantList result;
    if (!m_models) return result;
    const QVariantList models = m_models->filteredSttModels();
    for (const QVariant &entry : models) {
        const QVariantMap model = entry.toMap();
        const QString id = model.value(QStringLiteral("id")).toString().toLower();
        if (id.contains(QStringLiteral("qwen")) || id.contains(QStringLiteral("nemotron"))
            || id.contains(QStringLiteral("whisper"))) {
            if (!QFileInfo(model.value(QStringLiteral("path")).toString()).isFile()) continue;
            result.append(model);
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const QVariant &left, const QVariant &right) {
        auto score = [](const QVariantMap &item) {
            const QString file = item.value(QStringLiteral("fileName")).toString().toLower();
            if (file.contains(QStringLiteral("q4_k"))) return 0;
            if (file.endsWith(QStringLiteral(".bin"))) return 1;
            if (file.contains(QStringLiteral("q8_0"))) return 2;
            if (file.contains(QStringLiteral("f16"))) return 4;
            return 3;
        };
        return score(left.toMap()) < score(right.toMap());
    });
    return result;
}

double AlignmentExecutionService::averageConfidence() const
{
    if (m_segments.isEmpty()) return 0.0;
    double total = 0.0;
    for (const QVariant &entry : m_segments)
        total += entry.toMap().value(QStringLiteral("confidence")).toDouble();
    return total / double(m_segments.size());
}

int AlignmentExecutionService::segmentIndexAt(double seconds) const
{
    for (qsizetype i = 0; i < m_segments.size(); ++i) {
        const QVariantMap segment = m_segments.at(i).toMap();
        const double start = segment.value(QStringLiteral("start")).toDouble();
        const double end = segment.value(QStringLiteral("end"), start).toDouble();
        if (seconds >= start && seconds < end) return int(i);
    }
    return -1;
}

QVariantList AlignmentExecutionService::karaokeLines() const
{
    QVariantList lines;
    QVariantList words;
    double lineStart = 0.0;
    double lineEnd = 0.0;
    double confidenceTotal = 0.0;

    auto flush = [&]() {
        if (words.isEmpty()) return;
        lines.append(QVariantMap{{QStringLiteral("words"), words},
                                 {QStringLiteral("start"), lineStart},
                                 {QStringLiteral("end"), lineEnd},
                                 {QStringLiteral("confidence"), confidenceTotal / double(words.size())}});
        words.clear();
        confidenceTotal = 0.0;
    };

    for (qsizetype i = 0; i < m_segments.size(); ++i) {
        QVariantMap word = m_segments.at(i).toMap();
        const double start = word.value(QStringLiteral("start")).toDouble();
        const double end = word.value(QStringLiteral("end"), start).toDouble();
        const QString text = word.value(QStringLiteral("text")).toString().trimmed();
        if (!words.isEmpty() && (words.size() >= 10 || start - lineEnd > 0.8)) flush();
        if (words.isEmpty()) lineStart = start;
        word.insert(QStringLiteral("segmentIndex"), int(i));
        words.append(word);
        lineEnd = end;
        confidenceTotal += word.value(QStringLiteral("confidence")).toDouble();
        if (text.endsWith(QLatin1Char('.')) || text.endsWith(QLatin1Char('!'))
            || text.endsWith(QLatin1Char('?')) || text.endsWith(QLatin1Char(';'))) flush();
    }
    flush();
    return lines;
}

int AlignmentExecutionService::karaokeLineIndexAt(double seconds) const
{
    const QVariantList lines = karaokeLines();
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QVariantMap line = lines.at(i).toMap();
        if (seconds >= line.value(QStringLiteral("start")).toDouble()
            && seconds < line.value(QStringLiteral("end")).toDouble()) return int(i);
    }
    return -1;
}

bool AlignmentExecutionService::runStudioAlignment(const QVariantMap &request)
{
    const QString runtimeId = request.value(QStringLiteral("runtimeId")).toString();
    const QString runtimeVersion = request.value(QStringLiteral("runtimeVersion")).toString();
    const QString mode = request.value(QStringLiteral("mode"), QStringLiteral("canonical")).toString();
    const QString timestampUnit = request.value(QStringLiteral("timestampUnit"), QStringLiteral("word")).toString();
    const QString outputFormat = request.value(QStringLiteral("outputFormat"), QStringLiteral("json")).toString();
    Logger::info(QStringLiteral("AlignmentExecutionService"),
                 QStringLiteral("Studio alignment requested: mode=%1 runtime=%2 version=%3 model=%4 audioSelected=%5 sttModel=%6")
                     .arg(mode, runtimeId, runtimeVersion,
                          request.value(QStringLiteral("modelId")).toString(),
                          request.value(QStringLiteral("audioPath")).toString().isEmpty() ? QStringLiteral("false") : QStringLiteral("true"),
                          request.value(QStringLiteral("sttModel")).toMap().value(QStringLiteral("id")).toString()));

    if (!prepareWorkflow(request)) {
        const WorkflowPlanNode *failedNode = m_workflowSession.plan().firstBlockingNode();
        setError(failedNode && !failedNode->errorCode.isEmpty() ? failedNode->errorCode : QStringLiteral("WORKFLOW_NOT_READY"),
                 failedNode && !failedNode->statusText.isEmpty() ? failedNode->statusText : QStringLiteral("Alignment workflow is not ready."));
        return false;
    }

    const auto payload = std::dynamic_pointer_cast<const AlignmentWorkflowPayload>(m_workflowSession.resolution().payload);
    if (!payload) {
        setError(QStringLiteral("WORKFLOW_PAYLOAD_INVALID"), QStringLiteral("Alignment workflow payload is invalid."));
        return false;
    }
    if (payload->directProcessExecution) {
        return align(runtimeId, runtimeVersion,
                     request.value(QStringLiteral("modelId")).toString(),
                     request.value(QStringLiteral("audioPath")).toString(),
                     request.value(QStringLiteral("transcript")).toString(),
                     request.value(QStringLiteral("language")).toString(),
                     timestampUnit, outputFormat,
                     request.value(QStringLiteral("normalizeTranscript"), true).toBool());
    }

    const QVariantMap pipelineRequest = payload->executionRequest;
    const QVariantMap alignerConfiguration = pipelineRequest.value(QStringLiteral("alignerConfiguration")).toMap();
    const QVariantMap sttConfiguration = pipelineRequest.value(QStringLiteral("sttConfiguration")).toMap();
    Logger::info(QStringLiteral("AlignmentExecutionService"),
                 QStringLiteral("Automatic pipeline resolved: crispRuntime=%1 vadModel=%2 alignerModel=%3 sttModel=%4")
                     .arg(alignerConfiguration.value(QStringLiteral("runtimePath")).toString().isEmpty() ? QStringLiteral("missing") : QStringLiteral("ready"),
                          pipelineRequest.value(QStringLiteral("vadModelPath")).toString().isEmpty() ? QStringLiteral("missing") : QStringLiteral("ready"),
                          alignerConfiguration.value(QStringLiteral("modelPath")).toString().isEmpty() ? QStringLiteral("missing") : QStringLiteral("ready"),
                          sttConfiguration.value(QStringLiteral("modelPath")).toString().isEmpty() ? QStringLiteral("missing") : QStringLiteral("ready")));
    return startPipeline(pipelineRequest);
}

bool AlignmentExecutionService::prepareWorkflow(const QVariantMap &request)
{
    return m_workflowSession.prepare(m_workflowResolver, request);
}

bool AlignmentExecutionService::align(const QString &runtimeId,
                                     const QString &runtimeVersion,
                                     const QString &modelId,
                                     const QString &audioPath,
                                     const QString &transcript,
                                     const QString &language,
                                     const QString &timestampUnit,
                                     const QString &outputFormat,
                                     bool normalizeTranscript,
                                     int timeoutMs)
{
    if (processing()) return false;
    clearResult();

    const QString executable = m_runtimes
        ? m_runtimes->getRuntimeExecutablePathForVersion(runtimeId, runtimeVersion)
        : QString();
    const QString modelDir = m_models ? m_models->concreteModelDir(modelId) : QString();
    const QString audio = localPath(audioPath);

    if (!QFileInfo(executable).isFile()) {
        setError(QStringLiteral("RUNTIME_NOT_INSTALLED"), QStringLiteral("Selected alignment runtime is not installed."));
        return false;
    }
    if (modelId.trimmed().isEmpty()) {
        setError(QStringLiteral("MODEL_NOT_SELECTED"), QStringLiteral("No alignment model is selected."));
        return false;
    }
    if (!QFileInfo(modelDir).isDir()) {
        setError(QStringLiteral("MODEL_DIRECTORY_MISSING"), QStringLiteral("Selected alignment model is not installed."));
        return false;
    }
    if (!QFileInfo(audio).isFile()) {
        setError(QStringLiteral("AUDIO_FILE_MISSING"), QStringLiteral("The selected audio file does not exist."));
        return false;
    }
    if (transcript.trimmed().isEmpty()) {
        setError(QStringLiteral("TRANSCRIPT_EMPTY"), QStringLiteral("Transcript cannot be empty."));
        return false;
    }

    QJsonObject request{
        {QStringLiteral("operation"), QStringLiteral("align")},
        {QStringLiteral("audio"), QDir::fromNativeSeparators(audio)},
        {QStringLiteral("transcript"), transcript},
        {QStringLiteral("language"), language.trimmed().isEmpty() ? QStringLiteral("eng") : language.trimmed()},
        {QStringLiteral("model_dir"), QDir::fromNativeSeparators(modelDir)},
        {QStringLiteral("timestamp_unit"), timestampUnit},
        {QStringLiteral("output_format"), outputFormat},
        {QStringLiteral("normalize_transcript"), normalizeTranscript}
    };

    m_stdout.clear();
    m_stderr.clear();
    m_cancelled = false;
    m_terminalProcessError = false;
    m_statusText = QStringLiteral("Starting runtime");
    m_process.setProgram(executable);
    m_process.setWorkingDirectory(QFileInfo(executable).absolutePath());
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    m_process.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
    m_process.write("\n");
    m_process.closeWriteChannel();
    m_timeout.start(qBound(1000, timeoutMs, 3600000));
    emit stateChanged();
    return true;
}

void AlignmentExecutionService::cancel()
{
    if (m_pipelineProcessing) {
        m_pipelineCancel.storeRelease(true);
        m_statusText = QStringLiteral("Cancelling");
        emit stateChanged();
        return;
    }
    if (!processing()) return;
    m_cancelled = true;
    m_statusText = QStringLiteral("Cancelling");
    emit stateChanged();
    m_process.kill();
}

void AlignmentExecutionService::clearResult()
{
    m_errorCode.clear();
    m_errorMessage.clear();
    m_output.clear();
    m_segments.clear();
    m_duration = 0.0;
    m_statusText = QStringLiteral("Ready");
    m_terminalProcessError = false;
    m_stage.clear();
    m_progress = 0;
    m_diagnostics.clear();
    emit stateChanged();
    emit resultChanged();
}

bool AlignmentExecutionService::startPipeline(const QVariantMap &request)
{
    if (processing()) return false;
    clearResult();
    const QString audioPath = localPath(request.value(QStringLiteral("audioPath")).toString());
    const QString mode = request.value(QStringLiteral("mode"), QStringLiteral("canonical")).toString();
    if (!QFileInfo::exists(audioPath)) {
        setError(QStringLiteral("AUDIO_FILE_MISSING"), QStringLiteral("The selected audio file does not exist."));
        return false;
    }
    if (mode == QStringLiteral("canonical") && request.value(QStringLiteral("transcript")).toString().trimmed().isEmpty()) {
        setError(QStringLiteral("TRANSCRIPT_EMPTY"), QStringLiteral("Transcript cannot be empty in canonical mode."));
        return false;
    }
    m_pipelineProcessing = true;
    m_pipelineCancel.storeRelease(false);
    m_stage = QStringLiteral("decoding");
    m_progress = 2;
    m_statusText = QStringLiteral("Decoding audio");
    Logger::info(QStringLiteral("AlignmentExecutionService"), QStringLiteral("Automatic pipeline started; decoding audio."));
    emit stateChanged();

    m_pipelineDecoder = new SttAudioDecoder(this);
    connect(m_pipelineDecoder, &SttAudioDecoder::finished, this, [this, request](const QVector<float> &samples) {
        m_pipelineDecoder->deleteLater();
        m_pipelineDecoder = nullptr;
        Logger::info(QStringLiteral("AlignmentExecutionService"),
                     QStringLiteral("Audio decoded for alignment: samples=%1 durationMs=%2")
                         .arg(samples.size()).arg(samples.size() * 1000 / 16000));
        startPipelineWorker(samples, request);
    });
    connect(m_pipelineDecoder, &SttAudioDecoder::errorOccurred, this, [this](const QString &error) {
        m_pipelineDecoder->deleteLater();
        m_pipelineDecoder = nullptr;
        m_pipelineProcessing = false;
        setError(QStringLiteral("AUDIO_DECODE_FAILED"), error);
    });
    m_pipelineDecoder->startDecode(audioPath);
    return true;
}

void AlignmentExecutionService::startPipelineWorker(const QVector<float> &samples, const QVariantMap &request)
{
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher]() {
        const QVariantMap result = watcher->result();
        watcher->deleteLater();
        m_pipelineProcessing = false;
        if (result.value(QStringLiteral("cancelled")).toBool()) {
            setError(QStringLiteral("CANCELLED"), QStringLiteral("Alignment was cancelled."));
            return;
        }
        const QString error = result.value(QStringLiteral("error")).toString();
        if (!error.isEmpty()) {
            setError(result.value(QStringLiteral("errorCode"), QStringLiteral("PIPELINE_FAILED")).toString(), error);
            return;
        }
        m_segments = result.value(QStringLiteral("segments")).toList();
        m_diagnostics = result.value(QStringLiteral("diagnostics")).toList();
        m_duration = result.value(QStringLiteral("duration")).toDouble();
        m_output = result.value(QStringLiteral("output")).toString();
        m_stage = QStringLiteral("completed");
        m_progress = 100;
        m_statusText = QStringLiteral("Completed");
        Logger::info(QStringLiteral("AlignmentExecutionService"),
                     QStringLiteral("Automatic alignment completed: segments=%1 durationMs=%2 diagnostics=%3 outputBytes=%4")
                         .arg(m_segments.size()).arg(qRound64(m_duration * 1000.0))
                         .arg(m_diagnostics.size()).arg(m_output.toUtf8().size()));
        emit stateChanged(); emit resultChanged(); emit completed();
    });

    const QFuture<QVariantMap> future = QtConcurrent::run([this, samples, request]() -> QVariantMap {
        QVariantMap outcome;
        auto update = [this](const QString &stage, int progress, const QString &status) {
            QMetaObject::invokeMethod(this, [this, stage, progress, status]() {
                m_stage = stage; m_progress = progress; m_statusText = status; emit stateChanged();
                Logger::info(QStringLiteral("AlignmentExecutionService"),
                             QStringLiteral("Pipeline stage: stage=%1 progress=%2 status=%3").arg(stage).arg(progress).arg(status));
            }, Qt::QueuedConnection);
        };
        const QVariantMap aligner = request.value(QStringLiteral("alignerConfiguration")).toMap();
        const QVariantMap stt = request.value(QStringLiteral("sttConfiguration")).toMap();
        const QVariantMap vad = request.value(QStringLiteral("vadOptions")).toMap();
        const QString runtimePath = aligner.value(QStringLiteral("runtimePath"), request.value(QStringLiteral("runtimePath"))).toString();
        const QString alignmentExecutable = aligner.value(QStringLiteral("executable")).toString();
        const QString runtimeRoot = QFileInfo(runtimePath).isFile() ? QFileInfo(runtimePath).absolutePath() : runtimePath;
#ifdef Q_OS_WIN
        const QString runtimeLib = findFile(runtimeRoot, {QStringLiteral("crispasr.dll")});
#else
        const QString runtimeLib = findFile(runtimeRoot, {QStringLiteral("libcrispasr.so"), QStringLiteral("libcrispasr.dylib")});
#endif
        const QString vadModel = request.value(QStringLiteral("vadModelPath")).toString().isEmpty()
            ? findFile(runtimeRoot, {QStringLiteral("ggml-silero-v6.2.0.bin"), QStringLiteral("*silero*.gguf")})
            : request.value(QStringLiteral("vadModelPath")).toString();
        const QString alignerModel = aligner.value(QStringLiteral("modelPath")).toString();
        const QString sttModel = stt.value(QStringLiteral("modelPath")).toString();
        const QString sttBackend = stt.value(QStringLiteral("backend"), QStringLiteral("whisper")).toString();
        const QString sttLanguage = stt.value(
            QStringLiteral("language"), request.value(QStringLiteral("language"), QStringLiteral("en"))).toString();
        const bool sttUseGpu = stt.value(QStringLiteral("useGpu"), false).toBool();
        if (runtimeLib.isEmpty()) {
            outcome.insert(QStringLiteral("errorCode"), QStringLiteral("CRISPASR_RUNTIME_MISSING"));
            outcome.insert(QStringLiteral("error"), QStringLiteral("CrispASR runtime is required for Generate from audio mode."));
            return outcome;
        }
        if (!QFileInfo(vadModel).isFile()) {
            outcome.insert(QStringLiteral("errorCode"), QStringLiteral("VAD_MODEL_MISSING"));
            outcome.insert(QStringLiteral("error"), QStringLiteral("Silero VAD model is not installed. Reconfigure the alignment model to install it."));
            return outcome;
        }
        if (alignerModel.isEmpty() || !QFileInfo(alignerModel).exists()) {
            outcome.insert(QStringLiteral("errorCode"), QStringLiteral("ALIGNER_MODEL_MISSING"));
            outcome.insert(QStringLiteral("error"), QStringLiteral("The selected alignment model path is missing."));
            return outcome;
        }
        if (sttModel.isEmpty() || !QFileInfo(sttModel).isFile()) {
            outcome.insert(QStringLiteral("errorCode"), QStringLiteral("STT_MODEL_FILE_MISSING"));
            outcome.insert(QStringLiteral("error"), QStringLiteral("The selected STT anchor must resolve to a model file, not a directory."));
            return outcome;
        }
        CrispAlignmentInterface crisp;
        if (!crisp.load(runtimeLib)) {
            outcome.insert(QStringLiteral("errorCode"), QStringLiteral("RUNTIME_INCOMPATIBLE"));
            outcome.insert(QStringLiteral("error"), crisp.errorString());
            return outcome;
        }
        update(QStringLiteral("vad"), 10, QStringLiteral("Detecting speech"));
        const int threads = qMax(1, request.value(QStringLiteral("threads"), 4).toInt());
        const auto spans = crisp.vadSlices(vadModel, samples,
            float(vad.value(QStringLiteral("threshold"), 0.5).toDouble()),
            vad.value(QStringLiteral("minSpeechMs"), 250).toInt(),
            vad.value(QStringLiteral("minSilenceMs"), 300).toInt(),
            vad.value(QStringLiteral("speechPadMs"), 250).toInt(),
            float(vad.value(QStringLiteral("maxChunkSeconds"), 30.0).toDouble()), threads);
        if (spans.isEmpty()) {
            outcome.insert(QStringLiteral("errorCode"), QStringLiteral("NO_SPEECH"));
            outcome.insert(QStringLiteral("error"), QStringLiteral("No speech regions were detected."));
            return outcome;
        }
        QVector<QVector<float>> chunks;
        QStringList chunkTexts;
        chunks.reserve(spans.size());
        for (int i = 0; i < spans.size(); ++i) {
            if (m_pipelineCancel.loadAcquire()) { outcome.insert(QStringLiteral("cancelled"), true); return outcome; }
            const int begin = qBound(0, int(spans[i].start * 16000), samples.size());
            const int end = qBound(begin, int(spans[i].end * 16000), samples.size());
            chunks.append(samples.mid(begin, end - begin));
            update(QStringLiteral("transcribing"), 15 + (35 * i / spans.size()),
                   QStringLiteral("Transcribing speech chunk %1 of %2").arg(i + 1).arg(spans.size()));
            QString transcriptionError;
            chunkTexts.append(crisp.transcribe(sttModel, sttBackend,
                sttLanguage, chunks.back(), threads, sttUseGpu, &transcriptionError).trimmed());
            if (!transcriptionError.isEmpty()) {
                outcome.insert(QStringLiteral("errorCode"), QStringLiteral("STT_SESSION_FAILED"));
                outcome.insert(QStringLiteral("error"), transcriptionError);
                return outcome;
            }
        }

        bool hasTranscript = false;
        for (const QString &text : chunkTexts) {
            if (!text.isEmpty()) {
                hasTranscript = true;
                break;
            }
        }
        if (!hasTranscript) {
            outcome.insert(QStringLiteral("errorCode"), QStringLiteral("TRANSCRIPTION_EMPTY"));
            outcome.insert(QStringLiteral("error"),
                           QStringLiteral("Speech was detected, but the STT anchor model produced no transcript. Check the language and STT model, then try again."));
            return outcome;
        }

        const QString mode = request.value(QStringLiteral("mode"), QStringLiteral("canonical")).toString();
        QStringList lines;
        QVector<int> sourceChunkByLine;
        AlignmentMatchResult matching;
        if (mode == QStringLiteral("canonical")) {
            lines = request.value(QStringLiteral("transcript")).toString().split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
            for (QString &line : lines) line = line.trimmed();
            update(QStringLiteral("matching"), 55, QStringLiteral("Matching transcript to speech"));
            matching = AlignmentTranscriptMatcher::match(lines, chunkTexts,
                request.value(QStringLiteral("language"), QStringLiteral("en")).toString());
        } else {
            // Empty STT chunks are not valid generated transcript segments. Keep the
            // original VAD index so timestamps still refer to the correct audio span.
            for (int chunkIndex = 0; chunkIndex < chunkTexts.size(); ++chunkIndex) {
                if (chunkTexts[chunkIndex].isEmpty()) continue;
                lines.append(chunkTexts[chunkIndex]);
                sourceChunkByLine.append(chunkIndex);
            }
            matching.score = matching.coverage = 1.0;
        }

        if (mode == QStringLiteral("automatic")) {
            update(QStringLiteral("aligning"), 60, QStringLiteral("Aligning transcript words with MMS"));
            const QVariantMap aligned = runAlignmentProcess(
                alignmentExecutable,
                alignerModel,
                localPath(request.value(QStringLiteral("audioPath")).toString()),
                lines.join(QLatin1Char('\n')),
                request.value(QStringLiteral("language"), QStringLiteral("eng")).toString(),
                request.value(QStringLiteral("timestampUnit"), QStringLiteral("word")).toString(),
                request.value(QStringLiteral("outputFormat"), QStringLiteral("json")).toString(),
                request.value(QStringLiteral("normalizeTranscript"), true).toBool());
            if (!aligned.value(QStringLiteral("error")).toString().isEmpty()) {
                return aligned;
            }
            update(QStringLiteral("exporting"), 95, QStringLiteral("Preparing word-level alignment output"));
            return aligned;
        }

        QVariantList outputSegments, diagnostics;
        const auto canonicalTokens = AlignmentTranscriptMatcher::tokenizeCanonical(lines,
            request.value(QStringLiteral("language"), QStringLiteral("en")).toString());
        for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            if (m_pipelineCancel.loadAcquire()) { outcome.insert(QStringLiteral("cancelled"), true); return outcome; }
            int firstChunk = -1, lastChunk = -1, matchedTokens = 0, totalTokens = 0;
            for (const auto &token : canonicalTokens) if (token.line == lineIndex) ++totalTokens;
            if (mode == QStringLiteral("automatic"))
                firstChunk = lastChunk = sourceChunkByLine.value(lineIndex, -1);
            else {
                for (int c = 0; c < matching.canonicalTokensByChunk.size(); ++c) {
                    for (int tokenIndex : matching.canonicalTokensByChunk[c]) {
                        if (tokenIndex >= 0 && tokenIndex < canonicalTokens.size() && canonicalTokens[tokenIndex].line == lineIndex) {
                            if (firstChunk < 0) firstChunk = c;
                            lastChunk = c; ++matchedTokens;
                        }
                    }
                }
            }
            const double coverage = mode == QStringLiteral("automatic") ? 1.0 :
                (totalTokens > 0 ? double(matchedTokens) / totalTokens : 0.0);
            const double score = mode == QStringLiteral("automatic") ? 1.0 : matching.score * coverage;
            QVariantMap segment{{QStringLiteral("text"), lines[lineIndex]},
                                {QStringLiteral("matchScore"), score},
                                {QStringLiteral("coverage"), coverage},
                                {QStringLiteral("confidence"), score}};
            if (firstChunk < 0 || score < 0.5) {
                segment.insert(QStringLiteral("start"), -1.0); segment.insert(QStringLiteral("end"), -1.0);
                segment.insert(QStringLiteral("status"), QStringLiteral("unaligned"));
                segment.insert(QStringLiteral("timestampSource"), QStringLiteral("none"));
                diagnostics.append(QVariantMap{{QStringLiteral("segment"), lineIndex},
                    {QStringLiteral("code"), QStringLiteral("NO_RELIABLE_ANCHOR")}});
                outputSegments.append(segment); continue;
            }
            // Ambiguous mappings get one wider acoustic window retry. Canonical text is unchanged.
            if (mode == QStringLiteral("canonical") && score < 0.7) {
                firstChunk = qMax(0, firstChunk - 1);
                lastChunk = qMin(spans.size() - 1, lastChunk + 1);
            }
            const double start = spans[firstChunk].start, end = spans[lastChunk].end;
            const int sampleBegin = qBound(0, int(start * 16000), samples.size());
            const int sampleEnd = qBound(sampleBegin, int(end * 16000), samples.size());
            update(QStringLiteral("aligning"), 60 + (30 * lineIndex / qMax(1, lines.size())),
                   QStringLiteral("Aligning segment %1 of %2").arg(lineIndex + 1).arg(lines.size()));
            const auto words = crisp.align(alignerModel, lines[lineIndex], samples.mid(sampleBegin, sampleEnd - sampleBegin),
                                           qRound64(start * 100.0), threads);
            QVariantList wordList;
            for (const auto &word : words) wordList.append(QVariantMap{{QStringLiteral("word"), word.text},
                {QStringLiteral("start"), word.start}, {QStringLiteral("end"), word.end}});
            if (!words.isEmpty()) {
                segment.insert(QStringLiteral("start"), words.first().start);
                segment.insert(QStringLiteral("end"), words.last().end);
                segment.insert(QStringLiteral("status"), score >= 0.7 && coverage >= 0.75 ? QStringLiteral("aligned") : QStringLiteral("ambiguous"));
                segment.insert(QStringLiteral("timestampSource"), QStringLiteral("ctc"));
                segment.insert(QStringLiteral("words"), wordList);
            } else if (mode == QStringLiteral("automatic")) {
                segment.insert(QStringLiteral("start"), start); segment.insert(QStringLiteral("end"), end);
                segment.insert(QStringLiteral("status"), QStringLiteral("coarse"));
                segment.insert(QStringLiteral("timestampSource"), QStringLiteral("vad"));
                diagnostics.append(QVariantMap{{QStringLiteral("segment"), lineIndex},
                    {QStringLiteral("code"), QStringLiteral("CTC_FAILED_USING_VAD")}});
            } else {
                segment.insert(QStringLiteral("start"), -1.0); segment.insert(QStringLiteral("end"), -1.0);
                segment.insert(QStringLiteral("status"), QStringLiteral("unaligned"));
                segment.insert(QStringLiteral("timestampSource"), QStringLiteral("none"));
                diagnostics.append(QVariantMap{{QStringLiteral("segment"), lineIndex},
                    {QStringLiteral("code"), QStringLiteral("CTC_ALIGNMENT_FAILED")}});
            }
            outputSegments.append(segment);
        }

        update(QStringLiteral("exporting"), 95, QStringLiteral("Preparing alignment output"));
        QJsonArray array; for (const QVariant &v : outputSegments) array.append(QJsonObject::fromVariantMap(v.toMap()));
        const QString format = request.value(QStringLiteral("outputFormat"), QStringLiteral("json")).toString();
        QString output;
        if (format == QStringLiteral("json")) {
            output = QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("segments"), array}}).toJson(QJsonDocument::Indented));
        } else {
            const bool vtt = format == QStringLiteral("webvtt");
            if (vtt) output = QStringLiteral("WEBVTT\n\n");
            int cue = 1;
            for (const QVariant &v : outputSegments) {
                const QVariantMap s = v.toMap();
                if (s.value(QStringLiteral("start")).toDouble() < 0) continue;
                if (!vtt) output += QString::number(cue++) + QLatin1Char('\n');
                output += formatTimestamp(s.value(QStringLiteral("start")).toDouble(), vtt) + QStringLiteral(" --> ") +
                          formatTimestamp(s.value(QStringLiteral("end")).toDouble(), vtt) + QLatin1Char('\n') +
                          s.value(QStringLiteral("text")).toString() + QStringLiteral("\n\n");
            }
        }
        outcome.insert(QStringLiteral("segments"), outputSegments);
        outcome.insert(QStringLiteral("diagnostics"), diagnostics);
        outcome.insert(QStringLiteral("duration"), double(samples.size()) / 16000.0);
        outcome.insert(QStringLiteral("output"), output);
        return outcome;
    });
    watcher->setFuture(future);
}

void AlignmentExecutionService::setError(const QString &code, const QString &message)
{
    Logger::error(QStringLiteral("AlignmentExecutionService"),
                  QStringLiteral("Alignment failed: code=%1 message=%2 stage=%3 progress=%4")
                      .arg(code, message, m_stage).arg(m_progress));
    m_errorCode = code;
    m_errorMessage = message;
    m_statusText = QStringLiteral("Failed");
    emit stateChanged();
    emit failed(code, message);
}

void AlignmentExecutionService::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_timeout.stop();
    m_stdout.append(m_process.readAllStandardOutput());
    m_stderr.append(m_process.readAllStandardError());

    if (m_cancelled) {
        setError(QStringLiteral("CANCELLED"), QStringLiteral("Alignment was cancelled."));
        return;
    }
    if (m_terminalProcessError) {
        emit stateChanged();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(m_stdout.trimmed(), &parseError);
    if (!document.isObject()) {
        const QString technical = QString::fromUtf8(m_stderr).trimmed();
        Logger::error(QStringLiteral("AlignmentExecutionService"),
                      QStringLiteral("Invalid runtime response (exit %1): %2").arg(exitCode).arg(technical));
        setError(QStringLiteral("INVALID_RUNTIME_RESPONSE"), QStringLiteral("Alignment runtime returned an invalid response."));
        return;
    }

    const QJsonObject response = document.object();
    if (exitStatus != QProcess::NormalExit || exitCode != 0 || !response.value(QStringLiteral("ok")).toBool()) {
        const QJsonObject error = response.value(QStringLiteral("error")).toObject();
        const QString code = error.value(QStringLiteral("code")).toString(QStringLiteral("RUNTIME_FAILED"));
        const QString message = error.value(QStringLiteral("message")).toString(QStringLiteral("Alignment runtime failed."));
        const QString technical = QString::fromUtf8(m_stderr).trimmed();
        Logger::error(QStringLiteral("AlignmentExecutionService"),
                      QStringLiteral("Runtime failed (exit %1, code %2): %3")
                          .arg(exitCode)
                          .arg(code, technical));
        setError(code, message);
        return;
    }

    m_duration = response.value(QStringLiteral("duration")).toDouble();
    m_segments = response.value(QStringLiteral("segments")).toArray().toVariantList();
    m_output = response.value(QStringLiteral("output")).toString();
    if (m_output.isEmpty()) m_output = QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Indented));
    m_statusText = QStringLiteral("Completed");
    Logger::info(QStringLiteral("AlignmentExecutionService"),
                 QStringLiteral("Process alignment completed: segments=%1 durationMs=%2 outputBytes=%3")
                     .arg(m_segments.size()).arg(qRound64(m_duration * 1000.0)).arg(m_output.toUtf8().size()));
    emit stateChanged();
    emit resultChanged();
    emit completed();
}

} // namespace LAStudio
