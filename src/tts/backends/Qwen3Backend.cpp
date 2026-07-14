#include "Qwen3Backend.h"
#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/CrispQwen3TtsInterface.h>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QDir>
#include <QMutex>

#ifdef Q_OS_WIN
#include <windows.h>
#include <string>
#include <QHash>
#endif

namespace LAStudio {

static QMutex s_envMutex;

static void setCrtEnv(const char *name, const char *value)
{
    qputenv(name, value);
#ifdef Q_OS_WIN
    static QHash<QByteArray, QByteArray> envCache;
    QByteArray envString = QByteArray(name) + "=" + QByteArray(value);
    envCache[QByteArray(name)] = envString;
    const char *data = envCache[QByteArray(name)].constData();

    if (auto handle = GetModuleHandleW(L"ucrtbase.dll")) {
        using PutenvFunc = int(*)(const char*);
        if (auto putenv_func = reinterpret_cast<PutenvFunc>(GetProcAddress(handle, "_putenv"))) {
            putenv_func(data);
        }
    }
    if (auto handle = GetModuleHandleW(L"msvcrt.dll")) {
        using PutenvFunc = int(*)(const char*);
        if (auto putenv_func = reinterpret_cast<PutenvFunc>(GetProcAddress(handle, "_putenv"))) {
            putenv_func(data);
        }
    }
#endif
}

static void unsetCrtEnv(const char *name)
{
    qunsetenv(name);
#ifdef Q_OS_WIN
    SetEnvironmentVariableA(name, nullptr);
    if (HMODULE hUcrt = GetModuleHandleW(L"ucrtbase.dll")) {
        typedef int (*putenv_fn)(const char*);
        if (auto p_putenv = (putenv_fn)(void*)GetProcAddress(hUcrt, "_putenv")) {
            std::string envStr = std::string(name) + "=";
            p_putenv(envStr.c_str());
        }
    }
    if (HMODULE hMsvcrt = GetModuleHandleW(L"msvcrt.dll")) {
        typedef int (*putenv_fn)(const char*);
        if (auto p_putenv = (putenv_fn)(void*)GetProcAddress(hMsvcrt, "_putenv")) {
            std::string envStr = std::string(name) + "=";
            p_putenv(envStr.c_str());
        }
    }
#endif
}

static void setOrClearEnv(const char *name, const QByteArray &value, bool restore)
{
    if (restore) {
        setCrtEnv(name, value.constData());
    } else {
        unsetCrtEnv(name);
    }
}

static int qwen3MaxFramesFor(const QString &text, const QVariantMap &settings)
{
    if (settings.contains(QStringLiteral("max_codec_steps"))) {
        const int requestedFrames = settings.value(QStringLiteral("max_codec_steps")).toInt();
        if (requestedFrames > 0) {
            return qBound(16, requestedFrames, 400);
        }
    }

    // Qwen3 generates 12 codec frames per second.  Leave enough headroom for
    // natural pacing and final-word prosody; the former fixed 100-frame default
    // capped every request at about 8.3 seconds.
    const int wordEstimate = qMax(1, text.simplified().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size());
    const int estimatedSeconds = qBound(2, wordEstimate / 2 + 2, 25);
    return qBound(48, estimatedSeconds * 12, 300);
}

Qwen3Backend::~Qwen3Backend()
{
    unload();
}

bool Qwen3Backend::load(const QVariantMap &config, QString &error, QVariantList &schema)
{
    const QString originalRuntimePath = config.value("runtimePath").toString();
    const bool isCudaRuntime = originalRuntimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive);
    QString modelPath = PathUtils::toNativeShortPath(config.value("model").toString());
    QString codecPath = PathUtils::toNativeShortPath(config.value("codec").toString());
    QString runtimePath = PathUtils::toNativeShortPath(originalRuntimePath);

    auto& qi = CrispQwen3TtsInterface::instance();
    if (!runtimePath.isEmpty()) {
        if (!qi.load(runtimePath)) {
            error = QString("Failed to load CrispASR Qwen3-TTS runtime: ") + qi.errorString();
            Logger::error("Qwen3Backend", error);
            return false;
        }
    }

    if (!qi.isLoaded()) {
        error = QStringLiteral("No CrispASR Qwen3-TTS runtime loaded. Please select one in settings.");
        Logger::error("Qwen3Backend", error);
        return false;
    }

    crispasr_open_params_v1 params{};
    params.abi_version = 1;
    params.n_threads = InferenceThreadPolicy::recommendedThreadCount(
        isCudaRuntime ? InferenceBackendProfile::CrispAsrGpu : InferenceBackendProfile::CrispAsrCpu);
    params.use_gpu = isCudaRuntime ? 1 : 0;
    params.verbosity = 1;
    params.flash_attn = 1;
    params.n_gpu_layers = -1;

    QByteArray modelPathBytes = modelPath.toUtf8();
    if (isCudaRuntime) {
        QMutexLocker locker(&s_envMutex);
        m_restoreCodePredictorBackendEnv = qEnvironmentVariableIsSet("QWEN3_TTS_CP_BACKEND");
        m_previousCodePredictorBackendEnv = qgetenv("QWEN3_TTS_CP_BACKEND");

        // Some CUDA devices collapse the autoregressive code predictor into
        // repeated codec codes. Keep the talker and codec on CUDA, but pin
        // this small, stateful predictor to the CPU scheduler for correctness.
        // CrispASR reads this at session open and on every predictor step.
        setCrtEnv("QWEN3_TTS_CP_BACKEND", "cpu");
        m_codePredictorBackendEnvOverridden = true;
        Logger::info("Qwen3Backend", "CUDA safety fallback: CPU code predictor; talker and codec remain on CUDA.");
    }

    m_session = qi.crispasr_session_open_with_params(modelPathBytes.constData(), "qwen3-tts", &params);
    if (!m_session) {
        if (isCudaRuntime) {
            QMutexLocker locker(&s_envMutex);
            setOrClearEnv("QWEN3_TTS_CP_BACKEND", m_previousCodePredictorBackendEnv, m_restoreCodePredictorBackendEnv);
            m_codePredictorBackendEnvOverridden = false;
            m_restoreCodePredictorBackendEnv = false;
            m_previousCodePredictorBackendEnv.clear();
        }
        error = QStringLiteral("Failed to initialize Qwen3-TTS session via CrispASR runtime.");
        Logger::error("Qwen3Backend", error);
        return false;
    }

    bool isVoiceDesign = false;
    if (qi.crispasr_session_is_voice_design) {
        isVoiceDesign = qi.crispasr_session_is_voice_design(static_cast<crispasr_session*>(m_session)) != 0;
    }

    QString familyId = config.value(QStringLiteral("familyId")).toString();
    if (familyId.contains(QStringLiteral("voicedesign")) && !isVoiceDesign) {
        error = QStringLiteral("Loaded model is not a VoiceDesign model, but the family configuration requires VoiceDesign.");
        Logger::error("Qwen3Backend", error);
        unload();
        return false;
    }

    m_modelPath = modelPath;

    if (!codecPath.isEmpty()) {
        QMutexLocker locker(&s_envMutex);
        if (originalRuntimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive) ||
            originalRuntimePath.contains(QStringLiteral("vulkan"), Qt::CaseInsensitive)) {
            setCrtEnv("QWEN3_TTS_CODEC_GPU", "1");
        }
        QByteArray codecPathBytes = codecPath.toUtf8();
        qi.crispasr_session_set_codec_path(static_cast<crispasr_session*>(m_session), codecPathBytes.constData());
    }

    // Common TTS params
    QVariantMap temperature;
    temperature["id"] = "temperature";
    temperature["name"] = "Temperature";
    temperature["type"] = "float";
    temperature["min"] = 0.1;
    temperature["max"] = 2.0;
    temperature["default"] = 1.0;
    temperature["description"] = "Randomness of the generation.";
    schema.append(temperature);

    QVariantMap seed;
    seed["id"] = "seed";
    seed["name"] = "Seed";
    seed["type"] = "int";
    seed["min"] = -1;
    seed["max"] = 2147483647;
    seed["default"] = -1;
    seed["description"] = "Random seed for reproducibility. -1 for random.";
    schema.append(seed);

    bool isCustomVoice = false;
    if (qi.crispasr_session_is_custom_voice) {
        isCustomVoice = qi.crispasr_session_is_custom_voice(static_cast<crispasr_session*>(m_session));
    }

    if (isCustomVoice) {
        QVariantList speakerChoices;
        int nSpeakers = qi.crispasr_session_n_speakers(static_cast<crispasr_session*>(m_session));
        for (int i = 0; i < nSpeakers; ++i) {
            const char* name = qi.crispasr_session_get_speaker_name(static_cast<crispasr_session*>(m_session), i);
            if (name) {
                QVariantMap choice;
                choice["text"] = QString::fromUtf8(name);
                choice["value"] = QString::fromUtf8(name);
                speakerChoices.append(choice);
            }
        }

        if (!speakerChoices.isEmpty()) {
            QVariantMap voiceParam;
            voiceParam["id"] = "voice";
            voiceParam["name"] = "Speaker";
            voiceParam["type"] = "choice";
            voiceParam["choices"] = speakerChoices;
            voiceParam["default"] = speakerChoices.first().toMap()["value"];
            voiceParam["description"] = "Select a preset speaker for Qwen3-TTS CustomVoice.";
            schema.append(voiceParam);

            // Set default speaker
            QByteArray defSpeaker = voiceParam["default"].toString().toUtf8();
            if (!qi.crispasr_session_set_speaker_name ||
                qi.crispasr_session_set_speaker_name(static_cast<crispasr_session*>(m_session), defSpeaker.constData()) != 0) {
                error = QStringLiteral("Failed to set default Qwen3-TTS speaker.");
                Logger::error("Qwen3Backend", error);
                unload();
                return false;
            }
        }

        QVariantMap instruct;
        instruct["id"] = "instruct";
        instruct["name"] = "Style Instruction";
        instruct["type"] = "string";
        instruct["default"] = "";
        instruct["description"] = "Describe the desired speaking style (e.g., 'excited', 'whispering').";
        schema.append(instruct);
    }

    if (isVoiceDesign) {
        QVariantMap instruct;
        instruct["id"] = "instruct";
        instruct["name"] = "Voice Description";
        instruct["type"] = "string";
        instruct["default"] = "";
        instruct["description"] = "Describe the desired speaking style and voice characteristics in detail.";
        schema.append(instruct);
    }

    return true;
}

void Qwen3Backend::unload()
{
    if (m_session) {
        auto& qi = CrispQwen3TtsInterface::instance();
        if (qi.isLoaded()) {
            qi.crispasr_session_close(static_cast<crispasr_session*>(m_session));
        }
        m_session = nullptr;
    }
    if (m_codePredictorBackendEnvOverridden) {
        QMutexLocker locker(&s_envMutex);
        setOrClearEnv("QWEN3_TTS_CP_BACKEND", m_previousCodePredictorBackendEnv, m_restoreCodePredictorBackendEnv);
        m_codePredictorBackendEnvOverridden = false;
        m_restoreCodePredictorBackendEnv = false;
        m_previousCodePredictorBackendEnv.clear();
    }
    m_modelPath.clear();
}

void Qwen3Backend::cancelProcessing()
{
    m_cancelRequested = true;
}

bool Qwen3Backend::synthesize(const QString &text, float speed, const QVariantMap &settings, 
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(speed);
    m_cancelRequested = false;
    auto& qi = CrispQwen3TtsInterface::instance();
    if (!qi.isLoaded() || !m_session) {
        error = QStringLiteral("CrispASR Qwen3-TTS runtime was unloaded unexpectedly.");
        return false;
    }

    // Apply speaker if changed
    if (settings.contains("voice")) {
        QByteArray speaker = settings.value("voice").toString().toUtf8();
        if (!qi.crispasr_session_set_speaker_name ||
            qi.crispasr_session_set_speaker_name(static_cast<crispasr_session*>(m_session), speaker.constData()) != 0) {
            error = QStringLiteral("Failed to apply selected Qwen3-TTS voice: ") + settings.value("voice").toString();
            return false;
        }
    }

    if (settings.contains("instruct")) {
        QByteArray instruct = settings.value("instruct").toString().trimmed().toUtf8();
        if (!instruct.isEmpty()) {
            if (!qi.crispasr_session_set_instruct ||
                qi.crispasr_session_set_instruct(static_cast<crispasr_session*>(m_session), instruct.constData()) != 0) {
                error = QStringLiteral("Failed to apply Qwen3-TTS style instruction.");
                return false;
            }
        }
    }

    const int maxFrames = qwen3MaxFramesFor(text, settings);
    const int seed = settings.value(QStringLiteral("seed"), 0).toInt();
    const float temperature = settings.value(QStringLiteral("temperature"), 0.9f).toFloat();

    if (qi.crispasr_session_set_temperature) {
        const int rc = qi.crispasr_session_set_temperature(static_cast<crispasr_session*>(m_session),
                                                           temperature,
                                                           seed > 0 ? static_cast<uint64_t>(seed) : 0);
        if (rc != 0) {
            error = QStringLiteral("Failed to apply Qwen3-TTS temperature (rc=%1).").arg(rc);
            return false;
        }
    } else if (settings.contains("temperature") && qi.crispasr_session_set_float) {
        if (qi.crispasr_session_set_float(static_cast<crispasr_session*>(m_session), "temperature", temperature) != 0) {
            error = QStringLiteral("Failed to apply Qwen3-TTS temperature.");
            return false;
        }
    } else if (settings.contains("temperature")) {
        error = QStringLiteral("Qwen3-TTS runtime does not expose temperature configuration.");
        return false;
    }

    if (settings.contains("seed") && qi.crispasr_session_set_int) {
        if (qi.crispasr_session_set_int(static_cast<crispasr_session*>(m_session), "seed", seed) != 0) {
            error = QStringLiteral("Failed to apply Qwen3-TTS seed.");
            return false;
        }
    }
    if (settings.contains("length_scale")) {
        const float lengthScale = settings.value(QStringLiteral("length_scale"), 1.0f).toFloat();
        if (qAbs(lengthScale - 1.0f) > 0.0001f) {
            if (!qi.crispasr_session_set_float) {
                error = QStringLiteral("Qwen3-TTS runtime does not expose length_scale configuration.");
                return false;
            }

            const int rc = qi.crispasr_session_set_float(static_cast<crispasr_session*>(m_session),
                                                         "length_scale",
                                                         lengthScale);
            if (rc != 0) {
                error = rc == -2
                    ? QStringLiteral("Qwen3-TTS runtime does not support length_scale.")
                    : QStringLiteral("Failed to apply Qwen3-TTS length_scale (rc=%1).").arg(rc);
                return false;
            }
        }
    }
    if (qi.crispasr_session_set_int) {
        const int rc = qi.crispasr_session_set_int(static_cast<crispasr_session*>(m_session), "max_codec_steps", maxFrames);
        if (rc != 0) {
            Logger::warning("Qwen3Backend", QStringLiteral("Qwen3 max_codec_steps setter returned rc=%1; using QWEN3_TTS_MAX_FRAMES fallback.").arg(rc));
        }
    }
    if (m_cancelRequested.load()) {
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }

    QByteArray textBytes = text.toUtf8();
    int n = 0;
    
    QMutexLocker locker(&s_envMutex);

    const bool hadMaxFramesEnv = qEnvironmentVariableIsSet("QWEN3_TTS_MAX_FRAMES");
    const QByteArray previousMaxFramesEnv = qgetenv("QWEN3_TTS_MAX_FRAMES");
    const bool hadSkipEnv = qEnvironmentVariableIsSet("QWEN3_TTS_SKIP_REF_DECODE");
    const QByteArray previousSkipEnv = qgetenv("QWEN3_TTS_SKIP_REF_DECODE");
    const bool hadSeedEnv = qEnvironmentVariableIsSet("QWEN3_TTS_SEED");
    const QByteArray previousSeedEnv = qgetenv("QWEN3_TTS_SEED");

    setCrtEnv("QWEN3_TTS_MAX_FRAMES", QByteArray::number(maxFrames).constData());
    // CrispASR 0.8.4's normal path decodes only generated codec codes.  The
    // opt-out (0) concatenates reference and generated codes then trims the
    // PCM result, which exercises CUDA's chunked codec path and can produce
    // a degenerate repeated sound.  The codec is stateless, so this is
    // equivalent for the generated audio while avoiding that CUDA-only path.
    setCrtEnv("QWEN3_TTS_SKIP_REF_DECODE", "1");
    if (seed > 0) {
        setCrtEnv("QWEN3_TTS_SEED", QByteArray::number(seed).constData());
    } else {
        const int randomSeed = QRandomGenerator::global()->bounded(1, 999999);
        setCrtEnv("QWEN3_TTS_SEED", QByteArray::number(randomSeed).constData());
    }

    Logger::info("Qwen3Backend", QStringLiteral("Qwen3 synthesis params: temperature=%1 seed=%2 max_frames=%3 text_chars=%4")
                 .arg(temperature).arg(seed).arg(maxFrames).arg(text.length()));
    float *pcm = qi.crispasr_session_synthesize(static_cast<crispasr_session*>(m_session), textBytes.constData(), &n);
    
    setOrClearEnv("QWEN3_TTS_MAX_FRAMES", previousMaxFramesEnv, hadMaxFramesEnv);
    setOrClearEnv("QWEN3_TTS_SKIP_REF_DECODE", previousSkipEnv, hadSkipEnv);
    setOrClearEnv("QWEN3_TTS_SEED", previousSeedEnv, hadSeedEnv);

    if (m_cancelRequested.load()) {
        if (pcm) qi.crispasr_pcm_free(pcm);
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }
    
    if (!pcm || n <= 0) {
        if (pcm) qi.crispasr_pcm_free(pcm);
        error = QStringLiteral("Qwen3-TTS synthesis returned empty audio.");
        return false;
    }

    const int hardSampleLimit = qMax(24000, maxFrames * 24000 / 12 + 24000);
    if (n > hardSampleLimit) {
        Logger::warning("Qwen3Backend", QStringLiteral("Qwen3-TTS produced %1 samples, exceeding guarded limit %2. Truncating to prevent runaway output.")
                        .arg(n).arg(hardSampleLimit));
        n = hardSampleLimit;
    }

    samples.resize(n);
    memcpy(samples.data(), pcm, sizeof(float) * n);
    qi.crispasr_pcm_free(pcm);
    
    sampleRate = 24000;
    return true;
}

bool Qwen3Backend::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                               QVector<float> &samples, int &sampleRate, QString &error)
{
    auto& qi = CrispQwen3TtsInterface::instance();
    if (!qi.isLoaded() || !m_session) {
        error = QStringLiteral("CrispASR Qwen3-TTS runtime was unloaded unexpectedly.");
        return false;
    }

    bool isVoiceDesign = false;
    if (qi.crispasr_session_is_voice_design) {
        isVoiceDesign = qi.crispasr_session_is_voice_design(static_cast<crispasr_session*>(m_session)) != 0;
    }

    if (isVoiceDesign) {
        error = QStringLiteral("Qwen3 VoiceDesign does not support voice cloning.");
        return false;
    }

    QString refText = settings.value("ref_text").toString();
    if (refText.isEmpty()) {
        error = QStringLiteral("Qwen3-TTS voice cloning requires a reference transcript (ref_text). Please provide it in the input box.");
        return false;
    }
    if (m_cancelRequested.load()) {
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }

    QByteArray refPathBytes = QDir::toNativeSeparators(PathUtils::toNativeShortPath(referencePath)).toUtf8();
    QByteArray refTextBytes = refText.toUtf8();
    
    int rc = qi.crispasr_session_set_voice(static_cast<crispasr_session*>(m_session), refPathBytes.constData(), refTextBytes.constData());
    if (rc != 0) {
        error = QStringLiteral("Failed to set reference voice for Qwen3-TTS (rc=%1). Ensure the WAV file is valid and 24kHz.").arg(rc);
        return false;
    }

    return synthesize(text, 1.0f, settings, samples, sampleRate, error);
}

} // namespace LAStudio
