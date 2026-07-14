#include "OmnivoiceBackend.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include "audio/WavIO.h"
#include <runtimes/OmnivoiceInterface.h>
#include <QDir>
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
    if (!codecPath.isEmpty()) {
        params.codec_path = codecPathBytes.constData();
    }

    m_context = oi.ov_init(&params);
    if (!m_context) {
        error = QString::fromUtf8(oi.ov_last_error());
        Logger::error("OmnivoiceBackend", "Failed to initialize voice model: " + error);
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
    if (m_context) {
        auto& oi = OmnivoiceInterface::instance();
        if (oi.isLoaded()) {
            oi.ov_free((struct ov_context*)m_context);
        }
        m_context = nullptr;
    }
}

bool OmnivoiceBackend::synthesize(const QString &text, float speed, const QVariantMap &settings, 
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(speed);
    m_cancelRequested = false;
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
    auto& oi = OmnivoiceInterface::instance();
    if (!oi.isLoaded() || !m_context) {
        error = QStringLiteral("Omnivoice runtime was unloaded unexpectedly.");
        return false;
    }

    WavIO::WavData refData = WavIO::loadAsFloatMono24k(PathUtils::toNativeShortPath(referencePath));
    if (refData.samples.isEmpty()) {
        error = QStringLiteral("Failed to load reference audio: ") + PathUtils::toNativeShortPath(referencePath);
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

} // namespace LAStudio
