#include "NemotronSttBackend.h"

#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <runtimes/CrispNemotronSttInterface.h>

#include <QRegularExpression>

namespace LAStudio {

namespace {
QString cleanNemotronText(const char *text)
{
    QString result = text ? QString::fromUtf8(text) : QString();
    // CrispASR v0.8.0's session ABI can expose SentencePiece control tokens
    // (for example <en-US>, <vi-VN>, or <unk>) in the rendered segment.
    // These are decoder metadata, not user-facing transcription text.
    static const QRegularExpression specialToken(QStringLiteral("<[^>\\r\\n]+>"));
    result.remove(specialToken);
    return result.simplified();
}
}

NemotronSttBackend::~NemotronSttBackend()
{
    unloadModel();
}

bool NemotronSttBackend::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath, QString &error)
{
    unloadModel();
    auto &ci = CrispNemotronSttInterface::instance();
    const QString nativeRuntimePath = PathUtils::toNativeShortPath(runtimePath);
    if (!nativeRuntimePath.isEmpty() && !ci.load(nativeRuntimePath)) {
        error = QStringLiteral("Failed to load CrispASR Nemotron runtime: ") + ci.errorString();
        return false;
    }
    if (!ci.isLoaded()) {
        error = QStringLiteral("CrispASR v0.8.0 or later is required for Nemotron-3.5 ASR Streaming.");
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
    const QByteArray modelPathBytes = PathUtils::toNativeShortPath(modelPath).toUtf8();
    m_session = ci.crispasr_session_open_with_params(modelPathBytes.constData(), "nemotron", &params);
    if (!m_session) {
        error = QStringLiteral("Failed to initialize the Nemotron-3.5 ASR Streaming session.");
        return false;
    }
    Logger::info("NemotronSttBackend", "Nemotron-3.5 ASR Streaming model loaded.");
    return true;
}

void NemotronSttBackend::unloadModel()
{
    auto &ci = CrispNemotronSttInterface::instance();
    if (m_session && ci.isLoaded()) ci.crispasr_session_close(static_cast<crispasr_session *>(m_session));
    m_session = nullptr;
}

bool NemotronSttBackend::transcribe(const QVector<float> &samples, const QString &language, int threads, bool translate,
                                    const QVariantMap &settings, QString &fullText, QVariantList &segments, QString &error)
{
    Q_UNUSED(threads);
    Q_UNUSED(translate);
    if (!m_session) {
        error = QStringLiteral("No Nemotron model loaded.");
        return false;
    }
    auto &ci = CrispNemotronSttInterface::instance();
    if (!ci.isLoaded()) {
        error = QStringLiteral("CrispASR Nemotron runtime was unloaded unexpectedly.");
        return false;
    }
    if (ci.crispasr_session_set_beam_size) {
        const int beamSize = qBound(1, settings.value(QStringLiteral("beam_size"), 1).toInt(), 20);
        ci.crispasr_session_set_beam_size(static_cast<crispasr_session *>(m_session), beamSize);
    }

    // The session controller owns the language selected in the STT UI. Do not
    // let a stale dynamic-settings snapshot override it: Nemotron's prompt
    // kernel is language-conditioned, and an incorrect prompt degrades the
    // transcript severely (for example, Vietnamese prompt on English audio).
    const QString effectiveLanguage = language.trimmed();
    const bool autoLanguage = effectiveLanguage.isEmpty() || effectiveLanguage.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0;
    const QByteArray languageBytes = autoLanguage ? QByteArray() : effectiveLanguage.toUtf8();
    const auto result = ci.crispasr_session_transcribe_lang(static_cast<crispasr_session *>(m_session), samples.constData(),
                                                            samples.size(), autoLanguage ? nullptr : languageBytes.constData());
    if (!result) {
        error = QStringLiteral("Nemotron transcription failed via CrispASR.");
        return false;
    }

    QString text;
    QVariantList outputSegments;
    const int count = ci.crispasr_session_result_n_segments(result);
    for (int i = 0; i < count; ++i) {
        const char *segmentText = ci.crispasr_session_result_segment_text(result, i);
        const QString value = cleanNemotronText(segmentText);
        text += value;
        outputSegments.append(QVariantMap{
            {QStringLiteral("text"), value},
            {QStringLiteral("start"), static_cast<double>(ci.crispasr_session_result_segment_t0(result, i)) / 100.0},
            {QStringLiteral("end"), static_cast<double>(ci.crispasr_session_result_segment_t1(result, i)) / 100.0}
        });
    }
    ci.crispasr_session_result_free(result);
    fullText = text.trimmed();
    segments = outputSegments;
    Logger::info("NemotronSttBackend", QStringLiteral("Completed transcription for %1 samples.").arg(samples.size()));
    return true;
}

} // namespace LAStudio
