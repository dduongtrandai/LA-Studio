#include "VoxCpm2Backend.h"
#include "core/HardwareManager.h"
#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/CrispVoxCpm2Interface.h>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <cstring>

namespace LAStudio {

namespace {

QString sanitizedInstruction(QString instruct)
{
    instruct = instruct.simplified();
    instruct.remove(QRegularExpression(QStringLiteral("[\\r\\n()]+")));
    return instruct;
}

bool isGpuRuntime(const QString &runtimePath)
{
    return runtimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive) ||
           runtimePath.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive);
}

bool isVoxCpm2FullPrecisionModel(const QString &modelPath)
{
    const QFileInfo info(modelPath);
    const QString filename = info.fileName().toLower();
    if (filename.contains(QStringLiteral("f16")) ||
        filename.contains(QStringLiteral("fp16"))) {
        return true;
    }

    constexpr qint64 fullPrecisionBytes = 3LL * 1024LL * 1024LL * 1024LL;
    return info.exists() && info.size() >= fullPrecisionBytes;
}

bool isKnownUnsafeGpuRuntime(const QString &runtimePath)
{
    const QString lower = QDir::fromNativeSeparators(runtimePath).toLower();
    return lower.contains(QStringLiteral("crispasr")) &&
           lower.contains(QStringLiteral("cuda")) &&
           lower.contains(QStringLiteral("v0.7.1"));
}

} // namespace

VoxCpm2Backend::~VoxCpm2Backend()
{
    unload();
}

bool VoxCpm2Backend::load(const QVariantMap &config, QString &error, QVariantList &schema)
{
    const QString originalRuntimePath = config.value(QStringLiteral("runtimePath")).toString();
    const QString modelPath = PathUtils::toNativeShortPath(config.value(QStringLiteral("model")).toString());
    const QString runtimePath = PathUtils::toNativeShortPath(originalRuntimePath);
    const bool gpuRuntime = isGpuRuntime(originalRuntimePath);

    if (gpuRuntime && isKnownUnsafeGpuRuntime(originalRuntimePath)) {
        error = QStringLiteral("VoxCPM2 CUDA is disabled for CrispASR v0.7.1 because synthesis crashes inside ggml-base.dll. "
                               "Select a CPU CrispASR runtime or use another TTS model until a fixed CrispASR runtime is available.");
        Logger::warning(QStringLiteral("VoxCpm2Backend"), error);
        return false;
    }

    if (gpuRuntime && isVoxCpm2FullPrecisionModel(modelPath)) {
        const double vramTotalGb = HardwareManager::instance()->vramTotal();
        constexpr double minRecommendedVramGb = 8.0;
        if (vramTotalGb > 0.0 && vramTotalGb < minRecommendedVramGb) {
            error = QStringLiteral("VoxCPM2 F16 requires at least %1 GB VRAM with CUDA/Vulkan. "
                                   "Detected %2 GB VRAM, so CrispASR falls back to CPU and synthesis can appear stuck. "
                                   "Use voxcpm2-q4_k.gguf or a GPU with more VRAM.")
                        .arg(minRecommendedVramGb, 0, 'f', 0)
                        .arg(vramTotalGb, 0, 'f', 2);
            Logger::warning(QStringLiteral("VoxCpm2Backend"), error);
            return false;
        }
    }

    auto &ci = CrispVoxCpm2Interface::instance();
    if (!runtimePath.isEmpty()) {
        if (!ci.load(runtimePath)) {
            error = QStringLiteral("Failed to load CrispASR VoxCPM2 runtime: ") + ci.errorString();
            Logger::error(QStringLiteral("VoxCpm2Backend"), error);
            return false;
        }
    }

    if (!ci.isLoaded()) {
        error = QStringLiteral("No CrispASR VoxCPM2 runtime loaded. Please select one in settings.");
        Logger::error(QStringLiteral("VoxCpm2Backend"), error);
        return false;
    }

    crispasr_open_params_v1 params{};
    params.abi_version = 2;
    params.n_threads = InferenceThreadPolicy::recommendedThreadCount(
        gpuRuntime ? InferenceBackendProfile::CrispAsrGpu : InferenceBackendProfile::CrispAsrCpu);
    params.use_gpu = gpuRuntime;
    params.verbosity = 1;
    params.flash_attn = 1;
    params.n_gpu_layers = -1;

    const QByteArray modelPathBytes = modelPath.toUtf8();
    m_session = ci.crispasr_session_open_with_params(modelPathBytes.constData(), "voxcpm2-tts", &params);
    if (!m_session) {
        error = QStringLiteral("Failed to initialize VoxCPM2 session via CrispASR runtime.");
        Logger::error(QStringLiteral("VoxCpm2Backend"), error);
        return false;
    }

    m_modelPath = modelPath;

    QVariantMap seed;
    seed[QStringLiteral("id")] = QStringLiteral("seed");
    seed[QStringLiteral("name")] = QStringLiteral("Seed");
    seed[QStringLiteral("type")] = QStringLiteral("int");
    seed[QStringLiteral("min")] = -1;
    seed[QStringLiteral("max")] = 2147483647;
    seed[QStringLiteral("default")] = -1;
    seed[QStringLiteral("description")] = QStringLiteral("Random seed for reproducibility. -1 for random.");
    schema.append(seed);

    QVariantMap instruct;
    instruct[QStringLiteral("id")] = QStringLiteral("instruct");
    instruct[QStringLiteral("name")] = QStringLiteral("Voice Description");
    instruct[QStringLiteral("type")] = QStringLiteral("string");
    instruct[QStringLiteral("default")] = QString();
    instruct[QStringLiteral("description")] = QStringLiteral("VoxCPM2 voice design description encoded in the upstream parenthesized prompt format.");
    schema.append(instruct);

    return true;
}

void VoxCpm2Backend::unload()
{
    if (m_session) {
        auto &ci = CrispVoxCpm2Interface::instance();
        if (ci.isLoaded()) {
            ci.crispasr_session_close(static_cast<crispasr_session *>(m_session));
        }
        m_session = nullptr;
    }
    m_modelPath.clear();
}

void VoxCpm2Backend::cancelProcessing()
{
    m_cancelRequested = true;
}

QString VoxCpm2Backend::textWithVoiceInstruction(const QString &text, const QVariantMap &settings) const
{
    const QString instruct = sanitizedInstruction(settings.value(QStringLiteral("instruct")).toString());
    if (instruct.isEmpty()) {
        return text;
    }
    return QStringLiteral("(") + instruct + QStringLiteral(")") + text;
}

bool VoxCpm2Backend::applySeed(const QVariantMap &settings, QString &error)
{
    auto &ci = CrispVoxCpm2Interface::instance();
    if (!ci.crispasr_session_set_tts_seed) {
        return true;
    }

    int seed = settings.value(QStringLiteral("seed"), -1).toInt();
    if (seed <= 0) {
        seed = QRandomGenerator::global()->bounded(1, 2147483647);
    }

    const int rc = ci.crispasr_session_set_tts_seed(static_cast<crispasr_session *>(m_session),
                                                    static_cast<uint64_t>(seed));
    if (rc != 0 && rc != -2) {
        error = QStringLiteral("Failed to apply VoxCPM2 seed (rc=%1).").arg(rc);
        return false;
    }
    return true;
}

bool VoxCpm2Backend::synthesizeInternal(const QString &text, const QVariantMap &settings,
                                        QVector<float> &samples, int &sampleRate, QString &error)
{
    auto &ci = CrispVoxCpm2Interface::instance();
    if (!ci.isLoaded() || !m_session) {
        error = QStringLiteral("CrispASR VoxCPM2 runtime was unloaded unexpectedly.");
        return false;
    }

    if (!applySeed(settings, error)) {
        return false;
    }
    if (m_cancelRequested.load()) {
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }

    const QString preparedText = textWithVoiceInstruction(text, settings);
    const QByteArray textBytes = preparedText.toUtf8();

    int n = 0;
    auto synthesizeFn = ci.crispasr_session_synthesize_raw
                            ? ci.crispasr_session_synthesize_raw
                            : ci.crispasr_session_synthesize;
    float *pcm = synthesizeFn(static_cast<crispasr_session *>(m_session),
                              textBytes.constData(), &n);
    if (m_cancelRequested.load()) {
        if (pcm) ci.crispasr_pcm_free(pcm);
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }
    if (!pcm || n <= 0) {
        if (pcm) ci.crispasr_pcm_free(pcm);
        error = QStringLiteral("VoxCPM2 synthesis returned empty audio.");
        return false;
    }

    samples.resize(n);
    memcpy(samples.data(), pcm, sizeof(float) * n);
    ci.crispasr_pcm_free(pcm);

    // CrispASR's unified session TTS API decimates VoxCPM2's native 48 kHz
    // decoder output to 24 kHz before returning it. Treating this buffer as
    // 48 kHz makes playback run 2x too fast and sound noisy.
    sampleRate = 24000;
    return true;
}

bool VoxCpm2Backend::synthesize(const QString &text, float speed, const QVariantMap &settings,
                                QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(speed);
    m_cancelRequested = false;
    return synthesizeInternal(text, settings, samples, sampleRate, error);
}

bool VoxCpm2Backend::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings,
                                QVector<float> &samples, int &sampleRate, QString &error)
{
    m_cancelRequested = false;
    auto &ci = CrispVoxCpm2Interface::instance();
    if (!ci.isLoaded() || !m_session) {
        error = QStringLiteral("CrispASR VoxCPM2 runtime was unloaded unexpectedly.");
        return false;
    }

    const QByteArray refPathBytes = QDir::toNativeSeparators(PathUtils::toNativeShortPath(referencePath)).toUtf8();
    if (m_cancelRequested.load()) {
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }
    const int rc = ci.crispasr_session_set_voice(static_cast<crispasr_session *>(m_session),
                                                 refPathBytes.constData(), nullptr);
    if (rc != 0) {
        error = QStringLiteral("Failed to set VoxCPM2 reference voice (rc=%1). Ensure the WAV file is valid.").arg(rc);
        return false;
    }

    return synthesizeInternal(text, settings, samples, sampleRate, error);
}

} // namespace LAStudio
