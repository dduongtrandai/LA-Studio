#include "WhisperSttBackend.h"

#include "core/InferenceThreadPolicy.h"
#include "core/Logger.h"
#include <runtimes/WhisperInterface.h>
#include <algorithm>
#include <QElapsedTimer>
#include <QRegularExpression>

namespace LAStudio {

WhisperSttBackend::~WhisperSttBackend()
{
    unloadModel();
}

bool WhisperSttBackend::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath, QString &error)
{
    unloadModel();
    m_useGpu = useGpu;

    auto &wi = WhisperInterface::instance();

    if (!runtimePath.isEmpty()) {
        if (!wi.load(runtimePath)) {
            error = QStringLiteral("Failed to load whisper runtime: ") + wi.errorString();
            return false;
        }
    }

    if (!wi.isLoaded()) {
        error = QStringLiteral("No whisper runtime loaded. Please select a runtime in settings.");
        return false;
    }

    struct whisper_context_params cparams = wi.context_default_params();
    cparams.use_gpu = useGpu;

    m_ctx = wi.init_from_file(modelPath.toUtf8().constData(), cparams);
    if (!m_ctx) {
        unloadModel();
        error = QStringLiteral("Failed to load whisper model: ") + modelPath;
        return false;
    }

    return true;
}

void WhisperSttBackend::unloadModel()
{
    if (m_ctx) {
        auto &wi = WhisperInterface::instance();
        if (wi.isLoaded()) {
            wi.free_context(m_ctx);
        }
        m_ctx = nullptr;
    }
}

void WhisperSttBackend::cancelProcessing()
{
    m_abort = true;
}

bool WhisperSttBackend::transcribe(const QVector<float> &samples,
                                   const QString &language,
                                   int threads,
                                   bool translate,
                                   const QVariantMap &settings,
                                   QString &fullText,
                                   QVariantList &segments,
                                   QString &error)
{
    QElapsedTimer timer;
    timer.start();
    if (!m_ctx) {
        error = QStringLiteral("No model loaded");
        return false;
    }

    auto &wi = WhisperInterface::instance();
    if (!wi.isLoaded()) {
        error = QStringLiteral("Whisper runtime was unloaded unexpectedly.");
        return false;
    }

    m_abort = false;

    auto readBool = [&](const char *key, bool fallback) -> bool {
        if (!settings.contains(QLatin1String(key))) return fallback;
        return settings.value(QLatin1String(key)).toBool();
    };
    auto readInt = [&](const char *key, int fallback) -> int {
        if (!settings.contains(QLatin1String(key))) return fallback;
        bool ok = false;
        int v = settings.value(QLatin1String(key)).toInt(&ok);
        return ok ? v : fallback;
    };
    auto readFloat = [&](const char *key, float fallback) -> float {
        if (!settings.contains(QLatin1String(key))) return fallback;
        bool ok = false;
        float v = settings.value(QLatin1String(key)).toFloat(&ok);
        return ok ? v : fallback;
    };
    auto readString = [&](const char *key, const QString &fallback) -> QString {
        if (!settings.contains(QLatin1String(key))) return fallback;
        return settings.value(QLatin1String(key)).toString();
    };

    const QString effectiveLanguage = readString("language", language).trimmed();
    const bool useAutoLanguage = effectiveLanguage.isEmpty() || effectiveLanguage.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0;
    QByteArray langBytes = useAutoLanguage ? QByteArray() : effectiveLanguage.toUtf8();
    const int requestedThreads = readInt("threads", threads);
    const int safeThreads = InferenceThreadPolicy::recommendedThreadCount(
        m_useGpu ? InferenceBackendProfile::WhisperGpu : InferenceBackendProfile::WhisperCpu,
        requestedThreads);
    const bool effectiveTranslate = readBool("translate", translate);

    const QString strategy = readString("strategy", QStringLiteral("greedy")).trimmed().toLower();
    const whisper_sampling_strategy sampling =
        (strategy == QStringLiteral("beam_search")) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;

    whisper_full_params params = wi.full_default_params(sampling);
    params.language = useAutoLanguage ? nullptr : langBytes.constData();
    params.detect_language = readBool("detect_language", useAutoLanguage);
    params.n_threads = safeThreads;
    params.translate = effectiveTranslate;
    params.n_max_text_ctx = std::max(0, readInt("n_max_text_ctx", params.n_max_text_ctx));
    params.offset_ms = std::max(0, readInt("offset_ms", params.offset_ms));
    params.duration_ms = std::max(0, readInt("duration_ms", params.duration_ms));
    // Keep decoder history across Whisper's internal windows so punctuation
    // and sentence structure remain coherent for subtitle translation.
    params.no_context = readBool("no_context", false);
    params.no_timestamps = readBool("no_timestamps", params.no_timestamps);
    params.single_segment = readBool("single_segment", params.single_segment);
    params.print_special = readBool("print_special", params.print_special);
    params.print_progress = readBool("print_progress", false);
    params.print_realtime = readBool("print_realtime", params.print_realtime);
    params.print_timestamps = readBool("print_timestamps", false);
    // The subtitle pipeline rebuilds sentence boundaries from a continuous
    // word timeline, so token timing is part of the normal STT result.
    params.token_timestamps = readBool("token_timestamps", true);
    params.thold_pt = readFloat("thold_pt", params.thold_pt);
    params.thold_ptsum = readFloat("thold_ptsum", params.thold_ptsum);
    params.max_len = std::max(0, readInt("max_len", params.max_len));
    params.split_on_word = readBool("split_on_word", params.split_on_word);
    params.max_tokens = std::max(0, readInt("max_tokens", params.max_tokens));
    params.debug_mode = readBool("debug_mode", params.debug_mode);
    params.audio_ctx = std::max(0, readInt("audio_ctx", params.audio_ctx));
    params.suppress_blank = readBool("suppress_blank", params.suppress_blank);
    params.suppress_nst = readBool("suppress_nst", params.suppress_nst);
    params.temperature = readFloat("temperature", params.temperature);
    params.max_initial_ts = readFloat("max_initial_ts", params.max_initial_ts);
    params.length_penalty = readFloat("length_penalty", params.length_penalty);
    params.temperature_inc = readFloat("temperature_inc", params.temperature_inc);
    params.entropy_thold = readFloat("entropy_thold", params.entropy_thold);
    params.logprob_thold = readFloat("logprob_thold", params.logprob_thold);
    params.no_speech_thold = readFloat("no_speech_thold", params.no_speech_thold);
    params.greedy.best_of = std::max(1, readInt("best_of", params.greedy.best_of));
    params.beam_search.beam_size = std::max(1, readInt("beam_size", params.beam_search.beam_size));
    params.beam_search.patience = readFloat("beam_patience", params.beam_search.patience);

    params.abort_callback = [](void *data) {
        return static_cast<WhisperSttBackend*>(data)->m_abort.load();
    };
    params.abort_callback_user_data = this;

    QByteArray promptBytes;
    const QString initialPrompt = readString("initial_prompt", QString());
    if (!initialPrompt.isEmpty()) {
        promptBytes = initialPrompt.toUtf8();
        params.initial_prompt = promptBytes.constData();
    }

    Logger::debug("WhisperSttBackend", QString("Starting transcription. Input language: '%1', Resolved language: '%2', Threads: %3, Translate: %4, Samples: %5, Dynamic settings: %6")
                                          .arg(language)
                                          .arg(QString::fromUtf8(langBytes))
                                          .arg(safeThreads)
                                          .arg(effectiveTranslate)
                                          .arg(samples.size())
                                          .arg(settings.size()));

    int result = wi.full_run(m_ctx, params, samples.constData(), samples.size());
    if (result != 0) {
        Logger::error(QStringLiteral("WhisperSttBackend"),
                      QStringLiteral("whisper_full_run failed code=%1 elapsedMs=%2").arg(result).arg(timer.elapsed()));
        error = QStringLiteral("Whisper transcription failed with code: %1").arg(result);
        return false;
    }

    const int nSegments = wi.full_n_segments(m_ctx);
    QString outText;
    QVariantList outSegments;

    for (int i = 0; i < nSegments; ++i) {
        const char *text = wi.full_get_segment_text(m_ctx, i);
        const int64_t t0 = wi.full_get_segment_t0(m_ctx, i);
        const int64_t t1 = wi.full_get_segment_t1(m_ctx, i);

        const QString segText = text ? QString::fromUtf8(text) : QString();
        outText += segText;

        QVariantMap seg;
        seg[QStringLiteral("text")] = segText;
        seg[QStringLiteral("start")] = static_cast<double>(t0) / 100.0;
        seg[QStringLiteral("end")] = static_cast<double>(t1) / 100.0;

        QVariantList words;
        if (params.token_timestamps && wi.full_n_tokens && wi.full_get_token_text
            && wi.full_get_token_data) {
            QVariantMap currentWord;
            auto flushWord = [&]() {
                if (!currentWord.value(QStringLiteral("text")).toString().isEmpty())
                    words.append(currentWord);
                currentWord.clear();
            };

            const int nTokens = wi.full_n_tokens(m_ctx, i);
            for (int j = 0; j < nTokens; ++j) {
                const char *rawPiece = wi.full_get_token_text(m_ctx, i, j);
                if (!rawPiece) continue;
                const QString piece = QString::fromUtf8(rawPiece);
                const QString trimmed = piece.trimmed();
                if (trimmed.isEmpty() || (trimmed.startsWith(QStringLiteral("[_"))
                                          && trimmed.endsWith(QStringLiteral("_]"))))
                    continue;

                const whisper_token_data token = wi.full_get_token_data(m_ctx, i, j);
                if (token.t0 < 0 || token.t1 <= token.t0) continue;
                const bool startsWord = !piece.isEmpty() && piece.front().isSpace();
                const bool punctuationOnly =
                    trimmed.indexOf(QRegularExpression(QStringLiteral("[\\p{L}\\p{N}]"))) < 0;

                if (startsWord && !currentWord.isEmpty()) flushWord();
                if (currentWord.isEmpty()) {
                    currentWord.insert(QStringLiteral("text"), trimmed);
                    currentWord.insert(QStringLiteral("start"), double(token.t0) / 100.0);
                    currentWord.insert(QStringLiteral("end"), double(token.t1) / 100.0);
                    currentWord.insert(QStringLiteral("confidence"), double(token.p));
                } else {
                    QString word = currentWord.value(QStringLiteral("text")).toString();
                    word += punctuationOnly || !startsWord
                        ? trimmed : QStringLiteral(" ") + trimmed;
                    currentWord.insert(QStringLiteral("text"), word);
                    currentWord.insert(QStringLiteral("end"), double(token.t1) / 100.0);
                    currentWord.insert(QStringLiteral("confidence"),
                                       qMin(currentWord.value(QStringLiteral("confidence")).toDouble(),
                                            double(token.p)));
                }
            }
            flushWord();
        }
        if (!words.isEmpty()) seg.insert(QStringLiteral("words"), words);
        outSegments.append(seg);
    }

    fullText = outText.trimmed();
    segments = outSegments;
    Logger::info(QStringLiteral("WhisperSttBackend"),
                 QStringLiteral("Inference complete segments=%1 textChars=%2 elapsedMs=%3")
                     .arg(nSegments).arg(fullText.size()).arg(timer.elapsed()));
    return true;
}

} // namespace LAStudio
