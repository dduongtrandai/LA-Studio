#include "runtimes/CrispAlignmentInterface.h"
#include "core/Logger.h"
#include "core/PathUtils.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace LAStudio {

bool CrispAlignmentInterface::load(const QString &libraryPath)
{
    unload();
    const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(libraryPath));
    if (!QFileInfo::exists(cleanPath)) {
        m_error = QStringLiteral("CrispASR runtime library does not exist: %1").arg(cleanPath);
        return false;
    }
    const QStringList runtimeDirs = crispRuntimeDependencyDirs(cleanPath);
    crispPrependRuntimeDirsToPath(runtimeDirs);
#ifdef Q_OS_WIN
    m_preloadedDlls = crispPreloadRuntimeDlls(cleanPath, runtimeDirs);
#endif
    m_library.setFileName(cleanPath);
    m_library.setLoadHints(QLibrary::ExportExternalSymbolsHint);
    if (!m_library.load()) {
        m_error = m_library.errorString();
        return false;
    }
#define RESOLVE(member, name, type) member = reinterpret_cast<type>(m_library.resolve(name))
    RESOLVE(m_vadSlices, "crispasr_vad_slices", vad_slices_fn);
    RESOLVE(m_vadFree, "crispasr_vad_free", vad_free_fn);
    RESOLVE(m_alignWords, "crispasr_align_words_abi", align_words_fn);
    RESOLVE(m_resultCount, "crispasr_align_result_n_words", result_count_fn);
    RESOLVE(m_resultText, "crispasr_align_result_word_text", result_text_fn);
    RESOLVE(m_resultT0, "crispasr_align_result_word_t0", result_time_fn);
    RESOLVE(m_resultT1, "crispasr_align_result_word_t1", result_time_fn);
    RESOLVE(m_resultFree, "crispasr_align_result_free", result_free_fn);
    RESOLVE(m_sessionOpen, "crispasr_session_open_with_params", session_open_fn);
    RESOLVE(m_sessionClose, "crispasr_session_close", session_close_fn);
    RESOLVE(m_sessionTranscribe, "crispasr_session_transcribe_lang", session_transcribe_fn);
    RESOLVE(m_sessionCount, "crispasr_session_result_n_segments", session_count_fn);
    RESOLVE(m_sessionText, "crispasr_session_result_segment_text", session_text_fn);
    RESOLVE(m_sessionFree, "crispasr_session_result_free", session_free_fn);
#undef RESOLVE
    if (!m_vadSlices || !m_vadFree || !m_alignWords || !m_resultCount || !m_resultText ||
        !m_resultT0 || !m_resultT1 || !m_resultFree || !m_sessionOpen || !m_sessionClose ||
        !m_sessionTranscribe || !m_sessionCount || !m_sessionText || !m_sessionFree) {
        m_error = QStringLiteral("CrispASR runtime is missing the required VAD/alignment ABI.");
        unload();
        return false;
    }
    return true;
}

void CrispAlignmentInterface::unload()
{
    m_vadSlices = nullptr; m_vadFree = nullptr; m_alignWords = nullptr;
    m_resultCount = nullptr; m_resultText = nullptr; m_resultT0 = nullptr;
    m_resultT1 = nullptr; m_resultFree = nullptr;
    m_sessionOpen = nullptr; m_sessionClose = nullptr; m_sessionTranscribe = nullptr;
    m_sessionCount = nullptr; m_sessionText = nullptr; m_sessionFree = nullptr;
#ifdef Q_OS_WIN
    crispUnloadLibraryAndDependencies(m_library, m_preloadedDlls);
#else
    if (m_library.isLoaded()) m_library.unload();
#endif
}

QString CrispAlignmentInterface::transcribe(const QString &modelPath, const QString &backend,
                                             const QString &language, const QVector<float> &pcm,
                                             int threads, bool useGpu, QString *error) const
{
    if (!m_sessionOpen || pcm.isEmpty()) {
        if (error) *error = QStringLiteral("CrispASR session API is unavailable or the audio chunk is empty.");
        return {};
    }
    crispasr_open_params_v1 params{};
    params.abi_version = 1;
    params.n_threads = qMax(1, threads);
    params.use_gpu = useGpu ? 1 : 0;
    params.verbosity = 1;
    params.flash_attn = 1;
    params.n_gpu_layers = -1;
    const QByteArray model = PathUtils::toNativeShortPath(modelPath).toUtf8();
    const QByteArray backendBytes = backend.toUtf8();
    crispasr_session *session = m_sessionOpen(model.constData(), backendBytes.constData(), &params);
    if (!session) {
        if (error) {
            *error = QStringLiteral("Failed to open the %1 STT session with the selected %2 runtime.")
                .arg(backend, useGpu ? QStringLiteral("GPU") : QStringLiteral("CPU"));
        }
        Logger::error(QStringLiteral("CrispAlignmentInterface"),
                      QStringLiteral("STT session open failed: backend=%1 useGpu=%2 model=%3")
                          .arg(backend).arg(useGpu).arg(modelPath));
        return {};
    }
    const QByteArray lang = language.toUtf8();
    crispasr_session_result *native = m_sessionTranscribe(session, pcm.constData(), pcm.size(), lang.constData());
    QString text;
    if (native) {
        const int count = m_sessionCount(native);
        for (int i = 0; i < count; ++i) {
            const char *part = m_sessionText(native, i);
            if (part) text += QString::fromUtf8(part);
        }
        m_sessionFree(native);
    } else if (error) {
        *error = QStringLiteral("The %1 STT runtime failed while transcribing an audio chunk.").arg(backend);
    }
    m_sessionClose(session);
    if (backend.compare(QStringLiteral("nemotron"), Qt::CaseInsensitive) == 0) {
        static const QRegularExpression controlToken(QStringLiteral("<[^>\\r\\n]+>"));
        text.remove(controlToken);
    }
    return text.simplified();
}

QVector<CrispAlignmentInterface::Span> CrispAlignmentInterface::vadSlices(
    const QString &modelPath, const QVector<float> &pcm, float threshold, int minSpeechMs,
    int minSilenceMs, int speechPadMs, float maxChunkSeconds, int threads) const
{
    QVector<Span> result;
    if (!m_vadSlices || pcm.isEmpty()) return result;
    float *spans = nullptr;
    const QByteArray path = QDir::toNativeSeparators(modelPath).toUtf8();
    const int count = m_vadSlices(path.constData(), pcm.constData(), pcm.size(), 16000, threshold,
                                  minSpeechMs, minSilenceMs, speechPadMs, maxChunkSeconds,
                                  threads, &spans);
    if (count > 0 && spans) {
        result.reserve(count);
        for (int i = 0; i < count; ++i) result.append({spans[i * 2], spans[i * 2 + 1]});
    }
    if (spans) m_vadFree(spans);
    return result;
}

QVector<CrispAlignmentInterface::Word> CrispAlignmentInterface::align(
    const QString &modelPath, const QString &transcript, const QVector<float> &pcm,
    int64_t offsetCs, int threads) const
{
    QVector<Word> result;
    if (!m_alignWords || transcript.trimmed().isEmpty() || pcm.isEmpty()) return result;
    const QByteArray path = QDir::toNativeSeparators(modelPath).toUtf8();
    const QByteArray text = transcript.toUtf8();
    crispasr_align_result *native = m_alignWords(path.constData(), text.constData(), pcm.constData(),
                                                 pcm.size(), offsetCs, threads);
    if (!native) return result;
    const int count = m_resultCount(native);
    result.reserve(qMax(0, count));
    for (int i = 0; i < count; ++i) {
        const char *word = m_resultText(native, i);
        result.append({word ? QString::fromUtf8(word) : QString(),
                       double(m_resultT0(native, i)) / 100.0,
                       double(m_resultT1(native, i)) / 100.0});
    }
    m_resultFree(native);
    return result;
}

} // namespace LAStudio
