#include "VibevoiceBackend.h"
#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/CrispVibeVoiceInterface.h>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <cstring>

namespace LAStudio {

namespace {

QStringList findVoicePacks(const QString &modelPath, const QString &selectedVoice)
{
    QSet<QString> uniquePaths;

    const QString cleanSelected = QDir::toNativeSeparators(PathUtils::toNativeShortPath(selectedVoice));
    if (!cleanSelected.isEmpty() && QFileInfo::exists(cleanSelected)) {
        uniquePaths.insert(cleanSelected);
    }

    QFileInfo modelInfo(modelPath);
    QDir modelDir = modelInfo.dir();
    const QStringList voiceFiles = modelDir.entryList(
        {QStringLiteral("vibevoice-voice-*.gguf"), QStringLiteral("voice-*.gguf")},
        QDir::Files,
        QDir::Name);

    for (const QString &fileName : voiceFiles) {
        uniquePaths.insert(QDir::toNativeSeparators(modelDir.absoluteFilePath(fileName)));
    }

    QStringList paths = uniquePaths.values();
    std::sort(paths.begin(), paths.end());
    return paths;
}

QString voiceDisplayName(const QString &path)
{
    QString name = QFileInfo(path).fileName();
    name.remove(QStringLiteral(".gguf"), Qt::CaseInsensitive);
    name.remove(QStringLiteral("vibevoice-voice-"));
    name.remove(QStringLiteral("voice-"));
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    name.replace(QLatin1Char('-'), QLatin1Char(' '));
    return name.simplified();
}

} // namespace

VibevoiceBackend::~VibevoiceBackend()
{
    unload();
}

bool VibevoiceBackend::load(const QVariantMap &config, QString &error, QVariantList &schema)
{
    const QString modelPath = PathUtils::toNativeShortPath(config.value(QStringLiteral("model")).toString());
    const QString runtimePath = PathUtils::toNativeShortPath(config.value(QStringLiteral("runtimePath")).toString());
    const QString selectedVoice = PathUtils::toNativeShortPath(config.value(QStringLiteral("voice")).toString());
    const QString backend = config.value(QStringLiteral("backend"), QStringLiteral("vibevoice-tts")).toString();
    const QString backendName = backend.isEmpty() ? QStringLiteral("vibevoice-tts") : backend;

    QVariantMap steps;
    steps[QStringLiteral("id")] = QStringLiteral("n_diffusion_steps");
    steps[QStringLiteral("name")] = QStringLiteral("Diffusion Steps");
    steps[QStringLiteral("type")] = QStringLiteral("int");
    steps[QStringLiteral("min")] = 4;
    steps[QStringLiteral("max")] = 100;
    steps[QStringLiteral("default")] = 20;
    steps[QStringLiteral("description")] = QStringLiteral("DPM-Solver++ inference steps. Higher values are slower.");
    schema.append(steps);

    QVariantMap seed;
    seed[QStringLiteral("id")] = QStringLiteral("seed");
    seed[QStringLiteral("name")] = QStringLiteral("Seed");
    seed[QStringLiteral("type")] = QStringLiteral("int");
    seed[QStringLiteral("min")] = 0;
    seed[QStringLiteral("max")] = 2147483647;
    seed[QStringLiteral("default")] = 0;
    seed[QStringLiteral("description")] = QStringLiteral("Random seed for reproducible VibeVoice sampling. 0 uses the runtime default.");
    schema.append(seed);

    auto& vi = CrispVibeVoiceInterface::instance();
    if (!runtimePath.isEmpty()) {
        if (!vi.load(runtimePath)) {
            error = QStringLiteral("Failed to load CrispASR VibeVoice runtime: ") + vi.errorString();
            Logger::error("VibevoiceBackend", error);
            return false;
        }
    }

    if (!vi.isLoaded()) {
        error = QStringLiteral("No CrispASR VibeVoice runtime loaded. Please select one in settings.");
        Logger::error("VibevoiceBackend", error);
        return false;
    }

    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        error = QStringLiteral("VibeVoice requires an installed GGUF model file.");
        Logger::error("VibevoiceBackend", error);
        return false;
    }

    QByteArray modelPathBytes = modelPath.toUtf8();
    QByteArray backendBytes = backendName.toUtf8();

    crispasr_open_params_v1 params{};
    params.abi_version = 2;
    params.n_threads = InferenceThreadPolicy::recommendedThreadCount(InferenceBackendProfile::RealtimeTts);
    params.use_gpu = 0;
    params.verbosity = 1;
    params.flash_attn = 1;
    params.n_gpu_layers = -1;

    m_session = vi.crispasr_session_open_with_params(modelPathBytes.constData(), backendBytes.constData(), &params);
    if (!m_session) {
        error = QStringLiteral("Failed to initialize VibeVoice session via CrispASR runtime.");
        Logger::error("VibevoiceBackend", error);
        return false;
    }

    m_modelPath = modelPath;
    m_backendName = backendName;

    const QStringList voicePacks = findVoicePacks(modelPath, selectedVoice);
    if (voicePacks.isEmpty()) {
        error = QStringLiteral("VibeVoice realtime TTS requires a vibevoice-voice-*.gguf voice pack next to the model or selected as the Voice component.");
        Logger::error("VibevoiceBackend", error);
        unload();
        return false;
    }

    const QString preferredVoice = QDir::toNativeSeparators(PathUtils::toNativeShortPath(selectedVoice));
    m_currentVoicePath = (!preferredVoice.isEmpty() && QFileInfo::exists(preferredVoice))
        ? preferredVoice
        : voicePacks.first();
    QByteArray voiceBytes = m_currentVoicePath.toUtf8();
    if (vi.crispasr_session_set_voice(static_cast<crispasr_session *>(m_session), voiceBytes.constData(), nullptr) != 0) {
        error = QStringLiteral("Failed to load VibeVoice voice pack: ") + m_currentVoicePath;
        Logger::error("VibevoiceBackend", error);
        unload();
        return false;
    }

    QVariantList choices;
    for (const QString &voicePath : voicePacks) {
        QVariantMap choice;
        choice[QStringLiteral("text")] = voiceDisplayName(voicePath);
        choice[QStringLiteral("value")] = voicePath;
        choice[QStringLiteral("detail")] = QFileInfo(voicePath).fileName();
        choices.append(choice);
    }

    QVariantMap voiceParam;
    voiceParam[QStringLiteral("id")] = QStringLiteral("voice");
    voiceParam[QStringLiteral("name")] = QStringLiteral("Voice");
    voiceParam[QStringLiteral("type")] = QStringLiteral("choice");
    voiceParam[QStringLiteral("choices")] = choices;
    voiceParam[QStringLiteral("default")] = m_currentVoicePath;
    voiceParam[QStringLiteral("description")] = QStringLiteral("Selects the VibeVoice GGUF voice prompt used for synthesis.");
    schema.append(voiceParam);

    m_loaded = true;
    Logger::info("VibevoiceBackend", QStringLiteral("Loaded VibeVoice via CrispASR backend %1 with voice %2")
                                      .arg(m_backendName, m_currentVoicePath));
    return true;
}

void VibevoiceBackend::unload()
{
    if (m_session) {
        auto& vi = CrispVibeVoiceInterface::instance();
        if (vi.isLoaded() && vi.crispasr_session_close) {
            vi.crispasr_session_close(static_cast<crispasr_session *>(m_session));
        }
        m_session = nullptr;
    }
    m_loaded = false;
    m_modelPath.clear();
    m_backendName.clear();
    m_currentVoicePath.clear();
}

void VibevoiceBackend::cancelProcessing()
{
    m_cancelRequested = true;
}

bool VibevoiceBackend::synthesize(const QString &text, float speed, const QVariantMap &settings,
                                  QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(speed);
    m_cancelRequested = false;
    auto& vi = CrispVibeVoiceInterface::instance();
    if (!vi.isLoaded() || !m_loaded || !m_session) {
        error = QStringLiteral("CrispASR VibeVoice runtime was unloaded unexpectedly.");
        return false;
    }

    const QString requestedVoice = QDir::toNativeSeparators(PathUtils::toNativeShortPath(
        settings.value(QStringLiteral("voice")).toString()));
    if (!requestedVoice.isEmpty() && requestedVoice != m_currentVoicePath) {
        QByteArray voiceBytes = requestedVoice.toUtf8();
        if (vi.crispasr_session_set_voice(static_cast<crispasr_session *>(m_session), voiceBytes.constData(), nullptr) != 0) {
            error = QStringLiteral("Failed to switch VibeVoice voice pack to: ") + requestedVoice;
            Logger::error("VibevoiceBackend", error);
            return false;
        }
        m_currentVoicePath = requestedVoice;
    }

    if (settings.contains(QStringLiteral("seed")) && vi.crispasr_session_set_tts_seed) {
        const uint64_t seed = settings.value(QStringLiteral("seed")).toULongLong();
        if (seed > 0) {
            vi.crispasr_session_set_tts_seed(static_cast<crispasr_session *>(m_session), seed);
        }
    }

    if (settings.contains(QStringLiteral("n_diffusion_steps")) && vi.crispasr_session_set_tts_steps) {
        const int steps = settings.value(QStringLiteral("n_diffusion_steps"), 20).toInt();
        vi.crispasr_session_set_tts_steps(static_cast<crispasr_session *>(m_session), steps);
    }
    if (m_cancelRequested.load()) {
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }

    QByteArray textBytes = text.toUtf8();
    int n = 0;
    float *pcm = vi.crispasr_session_synthesize(static_cast<crispasr_session *>(m_session),
                                                textBytes.constData(),
                                                &n);
    if (m_cancelRequested.load()) {
        if (pcm) vi.crispasr_pcm_free(pcm);
        error = QStringLiteral("TTS synthesis was canceled.");
        return false;
    }
    if (!pcm || n <= 0) {
        if (pcm) vi.crispasr_pcm_free(pcm);
        error = QStringLiteral("VibeVoice synthesis returned empty audio.");
        Logger::error("VibevoiceBackend", error);
        return false;
    }

    samples.resize(n);
    std::memcpy(samples.data(), pcm, sizeof(float) * n);
    vi.crispasr_pcm_free(pcm);
    sampleRate = 24000;
    return true;
}

bool VibevoiceBackend::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings,
                                  QVector<float> &samples, int &sampleRate, QString &error)
{
    Q_UNUSED(text);
    Q_UNUSED(referencePath);
    Q_UNUSED(settings);
    Q_UNUSED(samples);
    Q_UNUSED(sampleRate);
    error = QStringLiteral("VibeVoice Realtime 0.5B in CrispASR uses preset GGUF voice packs and does not support dynamic WAV cloning.");
    return false;
}

} // namespace LAStudio
