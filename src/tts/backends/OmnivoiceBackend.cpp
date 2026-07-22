#include "OmnivoiceBackend.h"
#include "audio/AudioFileDecoder.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/OmnivoiceInterface.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <cstring>
#include <utility>

namespace LAStudio {

static QString s_sessionOmniRuntimePath;

OmnivoiceBackend::~OmnivoiceBackend()
{
    unload();
}

void OmnivoiceBackend::setProgressCallback(std::function<bool(int current,
                                                              int total,
                                                              const QString &stage,
                                                              int chunkIndex,
                                                              int chunkCount)> callback)
{
    m_progressCallback = std::move(callback);
}

bool OmnivoiceBackend::handleProgress(int current,
                                      int total,
                                      const char *stage,
                                      int chunkIndex,
                                      int chunkCount,
                                      void *userData)
{
    auto *self = static_cast<OmnivoiceBackend *>(userData);
    if (!self || self->m_cancelRequested.load()) {
        return false;
    }
    if (!self || !self->m_progressCallback) {
        return true;
    }
    return self->m_progressCallback(current,
                                    total,
                                    QString::fromUtf8(stage ? stage : ""),
                                    chunkIndex,
                                    chunkCount);
}

bool OmnivoiceBackend::shouldCancel(void *userData)
{
    auto *self = static_cast<OmnivoiceBackend *>(userData);
    return self && self->m_cancelRequested.load();
}

void OmnivoiceBackend::cancelProcessing()
{
    m_cancelRequested = true;
}

bool OmnivoiceBackend::load(const QVariantMap &config, QString &error, QVariantList &schema)
{
    QString modelPath = PathUtils::toNativeShortPath(config.value("model").toString());
    QString codecPath = PathUtils::toNativeShortPath(config.value("codec").toString());
    QString runtimePath = PathUtils::toNativeShortPath(config.value("runtimePath").toString());

    if (modelPath.isEmpty() || !QFileInfo(modelPath).isFile()) {
        error = QStringLiteral("OmniVoice base model file is missing: %1").arg(modelPath);
        Logger::error("OmnivoiceBackend", error);
        return false;
    }
    if (codecPath.isEmpty() || !QFileInfo(codecPath).isFile()) {
        error = QStringLiteral("OmniVoice audio codec file is missing: %1").arg(codecPath);
        Logger::error("OmnivoiceBackend", error);
        return false;
    }

    QVariantMap steps;
    steps["id"] = "mg_num_step";
    steps["name"] = "Inference Steps";
    steps["type"] = "int";
    steps["min"] = 10;
    steps["max"] = 100;
    steps["default"] = 32;
    steps["description"] = "Number of iterative decoding steps.";
    schema.append(steps);

    QVariantMap guidance;
    guidance["id"] = "mg_guidance_scale";
    guidance["name"] = "CFG Guidance";
    guidance["type"] = "float";
    guidance["min"] = 1.0;
    guidance["max"] = 5.0;
    guidance["default"] = 2.0;
    guidance["description"] = "Controls style adherence.";
    schema.append(guidance);

#ifdef Q_OS_WIN
    // Whisper, llama.cpp and OmniVoice ship different builds of DLLs with
    // identical names (ggml.dll, ggml-cuda.dll and cuBLAS). Windows resolves
    // dependent modules process-wide by basename, so loading OmniVoice after
    // translation may silently bind it to llama.cpp's incompatible GGML
    // binaries. The packaged CLI is a small official host for the same runtime;
    // running it out-of-process gives its DLL set an independent loader scope.
    const QString cliPath = QDir(QFileInfo(runtimePath).absolutePath())
                                .absoluteFilePath(QStringLiteral("omnivoice-tts.exe"));
    if (QFileInfo(cliPath).isFile()) {
        m_useIsolatedProcess = true;
        m_cliPath = cliPath;
        m_modelPath = modelPath;
        m_codecPath = codecPath;
        s_sessionOmniRuntimePath = runtimePath;
        Logger::info("OmnivoiceBackend",
                     QStringLiteral("OmniVoice configured with isolated worker process to avoid "
                                    "GGML/CUDA DLL collisions"));
        return true;
    }
#endif

    auto& oi = OmnivoiceInterface::instance();
    if (!runtimePath.isEmpty()) {
        if (!s_sessionOmniRuntimePath.isEmpty() &&
            s_sessionOmniRuntimePath != runtimePath) {
            error = QStringLiteral("Switching OmniVoice runtime backend (CPU/CUDA/Vulkan) in one running session is unstable. Please restart LA Studio after changing runtime.");
            Logger::error("OmnivoiceBackend", error + QStringLiteral(" Previous: %1 | Requested: %2")
                          .arg(s_sessionOmniRuntimePath, runtimePath));
            return false;
        }
        if (!oi.load(runtimePath)) {
            error = QString("Failed to load omnivoice runtime: ") + oi.errorString();
            Logger::error("OmnivoiceBackend", error);
            return false;
        }
    }

    if (!oi.isLoaded()) {
        error = QStringLiteral("No Omnivoice runtime loaded. Please select one in settings.");
        Logger::error("OmnivoiceBackend", error);
        return false;
    }

    QByteArray modelPathBytes = modelPath.toUtf8();
    QByteArray codecPathBytes = codecPath.toUtf8();

    ov_init_params params;
    oi.ov_init_default_params(&params);
    params.model_path = modelPathBytes.constData();
    params.codec_path = codecPathBytes.constData();

    // The runtime default enables Flash Attention. On pre-Ampere cards such
    // as RTX 20-series, a failed FA initialization can leave partially
    // allocated CUDA state behind. Retrying ov_init then fails later while
    // loading the BF16 codec even though that exact codec is valid. Start CUDA
    // sessions in the portable mode on the first attempt so initialization is
    // atomic and cannot poison a retry.
    const bool conservativeCuda =
        runtimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive);
    if (conservativeCuda) {
        params.use_fa = false;
        params.clamp_fp16 = true;
        Logger::info("OmnivoiceBackend",
                     QStringLiteral("Loading OmniVoice with conservative CUDA settings "
                                    "(Flash Attention disabled, FP16 clamp enabled)"));
    }

    m_context = oi.ov_init(&params);

    if (!m_context) {
        error = QString::fromUtf8(oi.ov_last_error());
        Logger::error("OmnivoiceBackend",
                      QStringLiteral("Failed to initialize voice model (model=%1, codec=%2): %3")
                          .arg(modelPath, codecPath, error));
        unload();
        return false;
    }

    if (!runtimePath.isEmpty())
        s_sessionOmniRuntimePath = runtimePath;
    Logger::info("OmnivoiceBackend", QString("Omnivoice loaded successfully"));
    return true;
}

void OmnivoiceBackend::unload()
{
    m_useIsolatedProcess = false;
    m_cliPath.clear();
    m_modelPath.clear();
    m_codecPath.clear();
    if (m_context) {
        auto& oi = OmnivoiceInterface::instance();
        if (oi.isLoaded()) {
            oi.ov_free((struct ov_context*)m_context);
        }
        m_context = nullptr;
    }
    OmnivoiceInterface::instance().unload();
    s_sessionOmniRuntimePath.clear();
}

bool OmnivoiceBackend::synthesize(const QString &text, float speed, const QVariantMap &settings, 
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(speed);
    m_cancelRequested = false;
    if (m_useIsolatedProcess) {
        return synthesizeIsolated(text, QString(), settings, samples, sampleRate, error);
    }
    auto& oi = OmnivoiceInterface::instance();
    if (!oi.isLoaded() || !m_context) {
        error = QStringLiteral("Omnivoice runtime was unloaded unexpectedly.");
        return false;
    }

    QByteArray textBytes = text.toUtf8();
    QByteArray langBytes;
    QByteArray instructBytes;

    ov_tts_params params;
    memset(&params, 0, sizeof(params));
    oi.ov_tts_default_params(&params);
    params.text = textBytes.constData();
    params.cancel = &OmnivoiceBackend::shouldCancel;
    params.cancel_user_data = this;
    if (params.abi_version >= 4) {
        params.on_progress = &OmnivoiceBackend::handleProgress;
        params.on_progress_user_data = this;
    }

    if (settings.contains(QStringLiteral("lang"))) {
        langBytes = settings.value(QStringLiteral("lang")).toString().toUtf8();
        params.lang = langBytes.constData();
    }

    if (settings.contains(QStringLiteral("instruct"))) {
        instructBytes = settings.value(QStringLiteral("instruct")).toString().toUtf8();
        params.instruct = instructBytes.constData();
    }

    if (settings.contains(QStringLiteral("mg_num_step"))) {
        params.mg_num_step = settings.value(QStringLiteral("mg_num_step")).toInt();
    }

    if (settings.contains(QStringLiteral("mg_guidance_scale"))) {
        params.mg_guidance_scale = settings.value(QStringLiteral("mg_guidance_scale")).toFloat();
    }

    if (settings.contains(QStringLiteral("mg_t_shift"))) {
        params.mg_t_shift = settings.value(QStringLiteral("mg_t_shift")).toFloat();
    }

    if (settings.contains(QStringLiteral("mg_layer_penalty_factor"))) {
        params.mg_layer_penalty_factor = settings.value(QStringLiteral("mg_layer_penalty_factor")).toFloat();
    }

    if (settings.contains(QStringLiteral("mg_position_temperature"))) {
        params.mg_position_temperature = settings.value(QStringLiteral("mg_position_temperature")).toFloat();
    }

    if (settings.contains(QStringLiteral("mg_class_temperature"))) {
        params.mg_class_temperature = settings.value(QStringLiteral("mg_class_temperature")).toFloat();
    }

    if (settings.contains(QStringLiteral("mg_seed"))) {
        params.mg_seed = settings.value(QStringLiteral("mg_seed")).toULongLong();
    }

    if (settings.contains(QStringLiteral("duration_sec"))) {
        const float durationSec = settings.value(QStringLiteral("duration_sec")).toFloat();
        if (durationSec > 0.0f && oi.ov_duration_sec_to_tokens) {
            const int durationTokens = oi.ov_duration_sec_to_tokens((struct ov_context*)m_context, durationSec);
            if (durationTokens > 0) {
                params.T_override = durationTokens;
            }
        }
    } else if (settings.contains(QStringLiteral("T_override"))) {
        params.T_override = settings.value(QStringLiteral("T_override")).toInt();
    }

    if (settings.contains(QStringLiteral("chunk_duration_sec"))) {
        params.chunk_duration_sec = settings.value(QStringLiteral("chunk_duration_sec")).toFloat();
    }

    if (settings.contains(QStringLiteral("chunk_threshold_sec"))) {
        params.chunk_threshold_sec = settings.value(QStringLiteral("chunk_threshold_sec")).toFloat();
    }

    ov_audio out;
    memset(&out, 0, sizeof(out));
    ov_status status = oi.ov_synthesize((struct ov_context*)m_context, &params, &out);

    if (status != OV_STATUS_OK) {
        error = QString::fromUtf8(oi.ov_last_error());
        return false;
    }

    samples.resize(out.n_samples);
    memcpy(samples.data(), out.samples, sizeof(float) * out.n_samples);
    sampleRate = out.sample_rate;

    oi.ov_audio_free(&out);
    return true;
}

bool OmnivoiceBackend::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    m_cancelRequested = false;
    if (m_useIsolatedProcess) {
        return synthesizeIsolated(text, referencePath, settings, samples, sampleRate, error);
    }
    auto& oi = OmnivoiceInterface::instance();
    if (!oi.isLoaded() || !m_context) {
        error = QStringLiteral("Omnivoice runtime was unloaded unexpectedly.");
        return false;
    }

    QString decodeError;
    WavIO::WavData refData = AudioFileDecoder::decodeMono(
        PathUtils::toNativeShortPath(referencePath), 24000, &decodeError);
    if (refData.samples.isEmpty()) {
        error = QStringLiteral("Failed to load reference audio: %1").arg(decodeError);
        Logger::error("OmnivoiceBackend", error);
        return false;
    }

    Logger::info("OmnivoiceBackend", QString("Reference audio loaded successfully: %1 samples @ %2 Hz")
                 .arg(refData.samples.size())
                 .arg(refData.sampleRate));

    const QVector<float> &refSamples = refData.samples;

    QByteArray textBytes = text.toUtf8();
    QByteArray langBytes;
    QByteArray instructBytes;
    QByteArray refTextBytes;

    ov_tts_params params;
    memset(&params, 0, sizeof(params));
    oi.ov_tts_default_params(&params);
    params.text = textBytes.constData();
    params.ref_audio_24k = refSamples.constData();
    params.ref_n_samples = refSamples.size();
    params.cancel = &OmnivoiceBackend::shouldCancel;
    params.cancel_user_data = this;
    if (params.abi_version >= 4) {
        params.on_progress = &OmnivoiceBackend::handleProgress;
        params.on_progress_user_data = this;
    }

    if (settings.contains("lang")) {
        langBytes = settings.value("lang").toString().toUtf8();
        params.lang = langBytes.constData();
    }

    if (settings.contains("instruct")) {
        instructBytes = settings.value("instruct").toString().toUtf8();
        params.instruct = instructBytes.constData();
    }

    if (settings.contains("ref_text")) {
        refTextBytes = settings.value("ref_text").toString().toUtf8();
        params.ref_text = refTextBytes.constData();
    }

    if (settings.contains("denoise")) {
        params.denoise = settings.value("denoise").toBool();
    }

    if (settings.contains("preprocess_prompt")) {
        params.preprocess_prompt = settings.value("preprocess_prompt").toBool();
    }

    if (settings.contains("mg_num_step")) {
        params.mg_num_step = settings.value("mg_num_step").toInt();
    }

    if (settings.contains("mg_guidance_scale")) {
        params.mg_guidance_scale = settings.value("mg_guidance_scale").toFloat();
    }

    if (settings.contains("mg_t_shift")) {
        params.mg_t_shift = settings.value("mg_t_shift").toFloat();
    }

    if (settings.contains("mg_layer_penalty_factor")) {
        params.mg_layer_penalty_factor = settings.value("mg_layer_penalty_factor").toFloat();
    }

    if (settings.contains("mg_position_temperature")) {
        params.mg_position_temperature = settings.value("mg_position_temperature").toFloat();
    }

    if (settings.contains("mg_class_temperature")) {
        params.mg_class_temperature = settings.value("mg_class_temperature").toFloat();
    }

    if (settings.contains("mg_seed")) {
        params.mg_seed = settings.value("mg_seed").toULongLong();
    }

    if (settings.contains("duration_sec")) {
        const float durationSec = settings.value("duration_sec").toFloat();
        if (durationSec > 0.0f && oi.ov_duration_sec_to_tokens) {
            const int durationTokens = oi.ov_duration_sec_to_tokens((struct ov_context*)m_context, durationSec);
            if (durationTokens > 0) {
                params.T_override = durationTokens;
            }
        }
    } else if (settings.contains("T_override")) {
        params.T_override = settings.value("T_override").toInt();
    }

    if (settings.contains("chunk_duration_sec")) {
        params.chunk_duration_sec = settings.value("chunk_duration_sec").toFloat();
    }

    if (settings.contains("chunk_threshold_sec")) {
        params.chunk_threshold_sec = settings.value("chunk_threshold_sec").toFloat();
    }

    ov_audio out;
    memset(&out, 0, sizeof(out));
    ov_status status = oi.ov_synthesize((struct ov_context*)m_context, &params, &out);

    if (status != OV_STATUS_OK) {
        error = QString::fromUtf8(oi.ov_last_error());
        Logger::error("OmnivoiceBackend", "Failed to synthesize cloned voice: " + error);
        return false;
    }

    samples.resize(out.n_samples);
    memcpy(samples.data(), out.samples, sizeof(float) * out.n_samples);
    sampleRate = out.sample_rate;

    oi.ov_audio_free(&out);
    Logger::info("OmnivoiceBackend", QString("Voice cloning successful"));
    return true;
}

bool OmnivoiceBackend::synthesizeIsolated(const QString &text,
                                          const QString &referencePath,
                                          const QVariantMap &settings,
                                          QVector<float> &samples,
                                          int &sampleRate,
                                          QString &error)
{
    if (!QFileInfo(m_cliPath).isFile()
        || !QFileInfo(m_modelPath).isFile()
        || !QFileInfo(m_codecPath).isFile()) {
        error = QStringLiteral("OmniVoice isolated worker configuration is incomplete.");
        return false;
    }

    QTemporaryDir temporaryDir(QStringLiteral("%1/lastudio-omnivoice-XXXXXX")
                                   .arg(QDir::tempPath()));
    if (!temporaryDir.isValid()) {
        error = QStringLiteral("Could not create temporary directory for OmniVoice worker.");
        return false;
    }

    const QString outputPath = QDir(temporaryDir.path()).absoluteFilePath(
        QStringLiteral("output.wav"));
    QStringList arguments{
        QStringLiteral("--model"), m_modelPath,
        QStringLiteral("--codec"), m_codecPath,
        QStringLiteral("--no-fa"),
        QStringLiteral("--clamp-fp16"),
        QStringLiteral("-o"), outputPath
    };

    const QString language = settings.value(QStringLiteral("lang")).toString().trimmed();
    if (!language.isEmpty())
        arguments << QStringLiteral("--lang") << language;
    const QString instruct = settings.value(QStringLiteral("instruct")).toString().trimmed();
    if (!instruct.isEmpty())
        arguments << QStringLiteral("--instruct") << instruct;

    float durationSec = settings.value(QStringLiteral("duration_sec")).toFloat();
    if (durationSec <= 0.0f && settings.value(QStringLiteral("T_override")).toInt() > 0)
        durationSec = settings.value(QStringLiteral("T_override")).toInt() / 25.0f;
    if (durationSec > 0.0f)
        arguments << QStringLiteral("--duration") << QString::number(durationSec, 'f', 3);

    if (settings.contains(QStringLiteral("mg_seed")))
        arguments << QStringLiteral("--seed")
                  << QString::number(settings.value(QStringLiteral("mg_seed")).toULongLong());
    if (settings.contains(QStringLiteral("chunk_duration_sec")))
        arguments << QStringLiteral("--chunk-duration")
                  << QString::number(settings.value(QStringLiteral("chunk_duration_sec")).toFloat());
    if (settings.contains(QStringLiteral("chunk_threshold_sec")))
        arguments << QStringLiteral("--chunk-threshold")
                  << QString::number(settings.value(QStringLiteral("chunk_threshold_sec")).toFloat());

    if (!referencePath.isEmpty()) {
        if (!QFileInfo(referencePath).isFile()) {
            error = QStringLiteral("OmniVoice reference audio is missing: %1").arg(referencePath);
            return false;
        }
        const QString referenceText = settings.value(QStringLiteral("ref_text")).toString().trimmed();
        if (referenceText.isEmpty()) {
            error = QStringLiteral("OmniVoice voice cloning requires a reference transcript.");
            return false;
        }
        const QString referenceTextPath = QDir(temporaryDir.path()).absoluteFilePath(
            QStringLiteral("reference.txt"));
        QFile referenceTextFile(referenceTextPath);
        if (!referenceTextFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || referenceTextFile.write(referenceText.toUtf8()) < 0) {
            error = QStringLiteral("Could not prepare OmniVoice reference transcript.");
            return false;
        }
        referenceTextFile.close();
        arguments << QStringLiteral("--ref-wav") << referencePath
                  << QStringLiteral("--ref-text") << referenceTextPath;
        if (settings.contains(QStringLiteral("denoise"))
            && !settings.value(QStringLiteral("denoise")).toBool())
            arguments << QStringLiteral("--no-denoise");
        if (settings.contains(QStringLiteral("preprocess_prompt"))
            && !settings.value(QStringLiteral("preprocess_prompt")).toBool())
            arguments << QStringLiteral("--no-preprocess-prompt");
    }

    QProcess process;
    process.setWorkingDirectory(QFileInfo(m_cliPath).absolutePath());
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(m_cliPath, arguments, QIODevice::ReadWrite);
    if (!process.waitForStarted(10000)) {
        error = QStringLiteral("Could not start isolated OmniVoice worker: %1")
                    .arg(process.errorString());
        return false;
    }
    process.write(text.toUtf8());
    process.write("\n");
    process.closeWriteChannel();

    QString workerLog;
    QByteArray pendingLog;
    int lastStep = 0;
    const QRegularExpression stepPattern(
        QStringLiteral("\\[MaskGIT-Step\\]\\s+(\\d+)/(\\d+)"));
    auto consumeLog = [&](const QByteArray &chunk) {
        pendingLog += chunk;
        qsizetype newline = -1;
        while ((newline = pendingLog.indexOf('\n')) >= 0) {
            const QString line = QString::fromUtf8(pendingLog.left(newline)).trimmed();
            pendingLog.remove(0, newline + 1);
            if (line.isEmpty()) continue;
            workerLog += line + QLatin1Char('\n');
            Logger::debug("OmnivoiceWorker", line);
            const QRegularExpressionMatch match = stepPattern.match(line);
            if (match.hasMatch()) {
                const int current = match.captured(1).toInt();
                const int total = match.captured(2).toInt();
                if (current > lastStep && m_progressCallback) {
                    lastStep = current;
                    if (!m_progressCallback(current, total, QStringLiteral("maskgit"), 1, 1))
                        m_cancelRequested = true;
                }
            }
        }
    };

    while (!process.waitForFinished(100)) {
        consumeLog(process.readAllStandardError());
        consumeLog(process.readAllStandardOutput());
        if (m_cancelRequested.load()) {
            process.terminate();
            if (!process.waitForFinished(2000)) process.kill();
            process.waitForFinished();
            error = QStringLiteral("OmniVoice generation was cancelled.");
            return false;
        }
    }
    consumeLog(process.readAllStandardError());
    consumeLog(process.readAllStandardOutput());
    if (!pendingLog.isEmpty()) workerLog += QString::fromUtf8(pendingLog).trimmed();

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QStringList lines = workerLog.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QString detail = lines.isEmpty() ? process.errorString() : lines.constLast();
        error = QStringLiteral("Isolated OmniVoice worker failed: %1").arg(detail);
        Logger::error("OmnivoiceBackend", error);
        return false;
    }

    const WavIO::WavData output = WavIO::loadAsFloat(outputPath);
    if (output.samples.isEmpty() || output.sampleRate <= 0) {
        error = QStringLiteral("OmniVoice worker completed without valid audio output.");
        return false;
    }
    samples = output.samples;
    sampleRate = output.sampleRate;
    Logger::info("OmnivoiceBackend",
                 QStringLiteral("Isolated OmniVoice worker generated %1 samples at %2 Hz")
                     .arg(samples.size()).arg(sampleRate));
    return true;
}

} // namespace LAStudio
