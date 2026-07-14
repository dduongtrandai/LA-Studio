#include "QwenSttBackend.h"
#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/CrispQwen3SttInterface.h>

namespace LAStudio {

QwenSttBackend::~QwenSttBackend()
{
    unloadModel();
}

bool QwenSttBackend::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath, QString &error)
{
    unloadModel();

    QString nativeModelPath = PathUtils::toNativeShortPath(modelPath);
    QString nativeRuntimePath = PathUtils::toNativeShortPath(runtimePath);

    auto &ci = CrispQwen3SttInterface::instance();
    if (!nativeRuntimePath.isEmpty()) {
        if (!ci.load(nativeRuntimePath)) {
            error = QStringLiteral("Failed to load CrispASR STT runtime: ") + ci.errorString();
            Logger::error("QwenSttBackend", error);
            return false;
        }
    }

    if (!ci.isLoaded()) {
        error = QStringLiteral("No CrispASR STT runtime loaded. Please select a runtime in settings.");
        Logger::error("QwenSttBackend", error);
        return false;
    }

    crispasr_open_params_v1 params{};
    params.abi_version = 1;
    params.n_threads = InferenceThreadPolicy::recommendedThreadCount(
        useGpu ? InferenceBackendProfile::CrispAsrGpu : InferenceBackendProfile::CrispAsrCpu);
    params.use_gpu = useGpu ? 1 : 0;
    params.verbosity = 1;
    params.flash_attn = 1;
    params.n_gpu_layers = -1;

    QByteArray modelPathBytes = nativeModelPath.toUtf8();
    m_session = ci.crispasr_session_open_with_params(modelPathBytes.constData(), "qwen3", &params);
    if (!m_session) {
        error = QStringLiteral("Failed to initialize Qwen3-ASR session via CrispASR runtime.");
        Logger::error("QwenSttBackend", error);
        return false;
    }

    m_modelPath = modelPath;
    Logger::info("QwenSttBackend", "Qwen3-ASR model loaded successfully.");
    return true;
}

void QwenSttBackend::unloadModel()
{
    if (m_session) {
        auto &ci = CrispQwen3SttInterface::instance();
        if (ci.isLoaded()) {
            ci.crispasr_session_close(static_cast<crispasr_session*>(m_session));
        }
        m_session = nullptr;
    }
    m_modelPath.clear();
}

bool QwenSttBackend::transcribe(const QVector<float> &samples,
                                const QString &language,
                                int threads,
                                bool translate,
                                const QVariantMap &settings,
                                QString &fullText,
                                QVariantList &segments,
                                QString &error)
{
    Q_UNUSED(threads);
    Q_UNUSED(translate);

    if (!m_session) {
        error = QStringLiteral("No model loaded");
        return false;
    }

    auto &ci = CrispQwen3SttInterface::instance();
    if (!ci.isLoaded()) {
        error = QStringLiteral("CrispASR STT runtime was unloaded unexpectedly.");
        return false;
    }

    float temperature = settings.value(QStringLiteral("temperature"), 0.0f).toFloat();
    int seed = settings.value(QStringLiteral("seed"), 0).toInt();
    int maxNewTokens = settings.value(QStringLiteral("max_new_tokens"), 0).toInt();
    int beamSize = settings.value(QStringLiteral("beam_size"), 1).toInt();
    float topP = settings.value(QStringLiteral("top_p"), 0.95f).toFloat();
    QString ask = settings.value(QStringLiteral("ask")).toString();

    if (ci.crispasr_session_set_temperature) {
        ci.crispasr_session_set_temperature(static_cast<crispasr_session*>(m_session), temperature, seed > 0 ? static_cast<uint64_t>(seed) : 0);
    }
    if (ci.crispasr_session_set_max_new_tokens) {
        ci.crispasr_session_set_max_new_tokens(static_cast<crispasr_session*>(m_session), maxNewTokens);
    }
    if (ci.crispasr_session_set_beam_size) {
        ci.crispasr_session_set_beam_size(static_cast<crispasr_session*>(m_session), beamSize);
    }
    if (ci.crispasr_session_set_top_p) {
        ci.crispasr_session_set_top_p(static_cast<crispasr_session*>(m_session), topP);
    }
    if (ci.crispasr_session_set_ask) {
        if (!ask.isEmpty()) {
            ci.crispasr_session_set_ask(static_cast<crispasr_session*>(m_session), ask.toUtf8().constData());
        } else {
            ci.crispasr_session_set_ask(static_cast<crispasr_session*>(m_session), "");
        }
    }

    const QString effectiveLanguage = settings.value(QStringLiteral("language"), language).toString().trimmed();
    const bool useAuto = effectiveLanguage.isEmpty() || effectiveLanguage.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0;
    QByteArray langBytes = useAuto ? QByteArray() : effectiveLanguage.toUtf8();
    const char* langPtr = useAuto ? nullptr : langBytes.constData();

    Logger::info("QwenSttBackend", QStringLiteral("Starting transcription. Language: %1, AutoDetect: %2, Samples: %3")
                                      .arg(effectiveLanguage)
                                      .arg(useAuto)
                                      .arg(samples.size()));

    crispasr_session_result* result = ci.crispasr_session_transcribe_lang(
        static_cast<crispasr_session*>(m_session), samples.constData(), samples.size(), langPtr);

    if (!result) {
        error = QStringLiteral("Transcription failed via CrispASR session.");
        Logger::error("QwenSttBackend", error);
        return false;
    }

    const int nSegments = ci.crispasr_session_result_n_segments(result);
    QString outText;
    QVariantList outSegments;

    for (int i = 0; i < nSegments; ++i) {
        const char *text = ci.crispasr_session_result_segment_text(result, i);
        const int64_t t0 = ci.crispasr_session_result_segment_t0(result, i);
        const int64_t t1 = ci.crispasr_session_result_segment_t1(result, i);

        const QString segText = text ? QString::fromUtf8(text) : QString();
        outText += segText;

        QVariantMap seg;
        seg[QStringLiteral("text")] = segText;
        seg[QStringLiteral("start")] = static_cast<double>(t0) / 100.0;
        seg[QStringLiteral("end")] = static_cast<double>(t1) / 100.0;
        outSegments.append(seg);
    }

    fullText = outText.trimmed();
    segments = outSegments;

    ci.crispasr_session_result_free(result);
    Logger::info("QwenSttBackend", QStringLiteral("Transcription completed. Characters: %1, Segments: %2").arg(fullText.length()).arg(segments.size()));
    return true;
}

} // namespace LAStudio
