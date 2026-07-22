#include "runtimes/LlamaTranslationInterface.h"

#include "core/Logger.h"
#include "core/PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QLocale>
#include <QRegularExpression>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
#include <utility>
#include <vector>

#include <ggml-backend.h>
#include <llama.h>

namespace LAStudio {
namespace {

template<typename T>
bool resolve(QLibrary &library, const char *name, T &function)
{
    function = reinterpret_cast<T>(library.resolve(name));
    return function != nullptr;
}

void prependRuntimePath(const QString &directory)
{
    QStringList entries = QString::fromLocal8Bit(qgetenv("PATH"))
                              .split(QDir::listSeparator(), Qt::SkipEmptyParts);
    const QString nativeDirectory = QDir::toNativeSeparators(directory);
    if (!entries.contains(nativeDirectory, Qt::CaseInsensitive)) {
        entries.prepend(nativeDirectory);
        qputenv("PATH", entries.join(QDir::listSeparator()).toLocal8Bit());
    }
}

QByteArray tokenPiece(const llama_vocab *vocab, llama_token token,
                      int32_t (*toPiece)(const llama_vocab *, llama_token, char *, int32_t, int32_t, bool))
{
    QByteArray buffer(128, Qt::Uninitialized);
    int32_t length = toPiece(vocab, token, buffer.data(), buffer.size(), 0, false);
    if (length < 0) {
        buffer.resize(-length);
        length = toPiece(vocab, token, buffer.data(), buffer.size(), 0, false);
    }
    return length > 0 ? QByteArray(buffer.constData(), length) : QByteArray();
}

QString fullLanguageName(const QString &language)
{
    const QString normalized = language.trimmed().replace(u'_', u'-');
    if (normalized.compare(QStringLiteral("zh-hant"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Traditional Chinese");
    }
    if (normalized.compare(QStringLiteral("yue"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Cantonese");
    }

    const QLocale locale(normalized);
    if (locale.language() != QLocale::C) {
        return QLocale::languageToString(locale.language());
    }
    return language.trimmed();
}

QString segmentMarker(int index)
{
    return QStringLiteral("[[LA_SEG_%1]]").arg(index, 6, 10, QLatin1Char('0'));
}

} // namespace

struct LlamaTranslationInterface::Api
{
    QLibrary ggml;
    QLibrary llama;
    llama_model *model = nullptr;

    decltype(&ggml_backend_load_all_from_path) backendLoadAllFromPath = nullptr;
    decltype(&llama_backend_init) backendInit = nullptr;
    decltype(&llama_backend_free) backendFree = nullptr;
    decltype(&llama_model_default_params) modelDefaultParams = nullptr;
    decltype(&llama_context_default_params) contextDefaultParams = nullptr;
    decltype(&llama_sampler_chain_default_params) samplerDefaultParams = nullptr;
    decltype(&llama_model_load_from_file) modelLoad = nullptr;
    decltype(&llama_model_free) modelFree = nullptr;
    decltype(&llama_model_get_vocab) modelGetVocab = nullptr;
    decltype(&llama_model_chat_template) modelChatTemplate = nullptr;
    decltype(&llama_chat_apply_template) chatApplyTemplate = nullptr;
    decltype(&llama_tokenize) tokenize = nullptr;
    decltype(&llama_init_from_model) contextInit = nullptr;
    decltype(&llama_free) contextFree = nullptr;
    decltype(&llama_batch_get_one) batchGetOne = nullptr;
    decltype(&llama_decode) decode = nullptr;
    decltype(&llama_vocab_is_eog) vocabIsEog = nullptr;
    decltype(&llama_token_to_piece) tokenToPiece = nullptr;
    decltype(&llama_sampler_chain_init) samplerChainInit = nullptr;
    decltype(&llama_sampler_chain_add) samplerChainAdd = nullptr;
    decltype(&llama_sampler_init_top_k) samplerTopK = nullptr;
    decltype(&llama_sampler_init_top_p) samplerTopP = nullptr;
    decltype(&llama_sampler_init_temp) samplerTemp = nullptr;
    decltype(&llama_sampler_init_penalties) samplerPenalties = nullptr;
    decltype(&llama_sampler_init_dist) samplerDist = nullptr;
    decltype(&llama_sampler_sample) samplerSample = nullptr;
    decltype(&llama_sampler_free) samplerFree = nullptr;

    bool resolveAll(QString *error)
    {
        const bool ok =
            resolve(ggml, "ggml_backend_load_all_from_path", backendLoadAllFromPath) &&
            resolve(llama, "llama_backend_init", backendInit) &&
            resolve(llama, "llama_backend_free", backendFree) &&
            resolve(llama, "llama_model_default_params", modelDefaultParams) &&
            resolve(llama, "llama_context_default_params", contextDefaultParams) &&
            resolve(llama, "llama_sampler_chain_default_params", samplerDefaultParams) &&
            resolve(llama, "llama_model_load_from_file", modelLoad) &&
            resolve(llama, "llama_model_free", modelFree) &&
            resolve(llama, "llama_model_get_vocab", modelGetVocab) &&
            resolve(llama, "llama_model_chat_template", modelChatTemplate) &&
            resolve(llama, "llama_chat_apply_template", chatApplyTemplate) &&
            resolve(llama, "llama_tokenize", tokenize) &&
            resolve(llama, "llama_init_from_model", contextInit) &&
            resolve(llama, "llama_free", contextFree) &&
            resolve(llama, "llama_batch_get_one", batchGetOne) &&
            resolve(llama, "llama_decode", decode) &&
            resolve(llama, "llama_vocab_is_eog", vocabIsEog) &&
            resolve(llama, "llama_token_to_piece", tokenToPiece) &&
            resolve(llama, "llama_sampler_chain_init", samplerChainInit) &&
            resolve(llama, "llama_sampler_chain_add", samplerChainAdd) &&
            resolve(llama, "llama_sampler_init_top_k", samplerTopK) &&
            resolve(llama, "llama_sampler_init_top_p", samplerTopP) &&
            resolve(llama, "llama_sampler_init_temp", samplerTemp) &&
            resolve(llama, "llama_sampler_init_penalties", samplerPenalties) &&
            resolve(llama, "llama_sampler_init_dist", samplerDist) &&
            resolve(llama, "llama_sampler_sample", samplerSample) &&
            resolve(llama, "llama_sampler_free", samplerFree);
        if (!ok && error) *error = QStringLiteral("The llama.cpp runtime does not expose the required b10036 C ABI.");
        return ok;
    }
};

LlamaTranslationInterface::LlamaTranslationInterface() = default;
LlamaTranslationInterface::~LlamaTranslationInterface() { unload(); }

void LlamaTranslationInterface::setError(const QString &message, QString *error)
{
    m_error = message;
    if (error) *error = message;
}

bool LlamaTranslationInterface::load(const QString &libraryPath,
                                     const QString &modelPath,
                                     QString *error,
                                     bool useGpu)
{
    unload();
    if (!QFileInfo(libraryPath).isFile()) {
        setError(QStringLiteral("The official llama.cpp library is missing."), error);
        return false;
    }
    if (!QFileInfo(modelPath).isFile()) {
        setError(QStringLiteral("The translation model file is missing."), error);
        return false;
    }

    const QFileInfo llamaInfo(libraryPath);
    prependRuntimePath(llamaInfo.absolutePath());
    m_api = std::make_unique<Api>();
    m_api->ggml.setFileName(QDir(llamaInfo.absolutePath()).absoluteFilePath(QStringLiteral("ggml.dll")));
    m_api->ggml.setLoadHints(QLibrary::ExportExternalSymbolsHint);
    if (!m_api->ggml.load()) {
        setError(QStringLiteral("Failed to load ggml.dll: %1").arg(m_api->ggml.errorString()), error);
        unload();
        return false;
    }
    m_api->llama.setFileName(llamaInfo.absoluteFilePath());
    m_api->llama.setLoadHints(QLibrary::ExportExternalSymbolsHint);
    if (!m_api->llama.load()) {
        setError(QStringLiteral("Failed to load llama.dll: %1").arg(m_api->llama.errorString()), error);
        unload();
        return false;
    }
    QString abiError;
    if (!m_api->resolveAll(&abiError)) {
        setError(abiError, error);
        unload();
        return false;
    }

    const QByteArray nativeRuntimePath =
        PathUtils::toNativeShortPath(llamaInfo.absolutePath()).toUtf8();
    m_api->backendLoadAllFromPath(nativeRuntimePath.constData());
    m_api->backendInit();
    llama_model_params params = m_api->modelDefaultParams();
    params.n_gpu_layers = useGpu ? -1 : 0;
    const QByteArray nativeModelPath = PathUtils::toNativeShortPath(modelPath).toUtf8();
    m_api->model = m_api->modelLoad(nativeModelPath.constData(), params);
    // A GPU runtime may be installed while the selected device cannot load
    // this quantization. Retry on CPU before surfacing a hard model error.
    if (!m_api->model && useGpu) {
        params.n_gpu_layers = 0;
        m_api->model = m_api->modelLoad(nativeModelPath.constData(), params);
    }
    if (!m_api->model) {
        setError(QStringLiteral("llama.cpp failed to load the translation model: %1")
                     .arg(QDir::toNativeSeparators(modelPath)), error);
        unload();
        return false;
    }
    m_modelPath = QFileInfo(modelPath).absoluteFilePath();
    m_cancelled.store(false, std::memory_order_relaxed);
    return true;
}

void LlamaTranslationInterface::unload()
{
    m_cancelled.store(true, std::memory_order_relaxed);
    if (!m_api) return;
    if (m_api->model && m_api->modelFree) m_api->modelFree(m_api->model);
    m_api->model = nullptr;
    if (m_api->backendFree) m_api->backendFree();
    if (m_api->llama.isLoaded()) m_api->llama.unload();
    if (m_api->ggml.isLoaded()) m_api->ggml.unload();
    m_api.reset();
    m_modelPath.clear();
}

void LlamaTranslationInterface::cancel() { m_cancelled.store(true, std::memory_order_relaxed); }
bool LlamaTranslationInterface::isLoaded() const { return m_api && m_api->model; }

bool LlamaTranslationInterface::parseContextTranslation(
    const QString &output, int expectedCount, QStringList *translations)
{
    if (!translations || expectedCount <= 0) return false;
    translations->clear();
    const QRegularExpression markerExpression(
        QStringLiteral("\\[\\[LA_SEG_(\\d{6})\\]\\]"));
    QVector<QRegularExpressionMatch> matches;
    auto iterator = markerExpression.globalMatch(output);
    while (iterator.hasNext()) matches.append(iterator.next());
    if (matches.size() != expectedCount) return false;

    for (int i = 0; i < matches.size(); ++i) {
        if (matches.at(i).captured(1).toInt() != i + 1) return false;
    }
    for (int i = 0; i < matches.size(); ++i) {
        const qsizetype start = matches.at(i).capturedEnd();
        const qsizetype end = i + 1 < matches.size()
            ? matches.at(i + 1).capturedStart() : output.size();
        const QString translated = output.mid(start, end - start).trimmed();
        if (translated.isEmpty()) {
            translations->clear();
            return false;
        }
        translations->append(translated);
    }
    return true;
}

QStringList LlamaTranslationInterface::translateBatch(
    const QStringList &texts, const QString &sourceLanguage, const QString &targetLanguage,
    int maxTokens, const std::shared_ptr<std::atomic_bool> &cancelToken, QString *error,
    const QString &task, const QVariantList &segments)
{
    if (!isLoaded() || texts.isEmpty()) {
        setError(QStringLiteral("The llama.cpp DLL runtime is not loaded."), error);
        return {};
    }

    const QString sourceName = fullLanguageName(sourceLanguage);
    const QString targetName = fullLanguageName(targetLanguage);
    Q_UNUSED(task);
    Q_UNUSED(segments);

    QStringList results;
    const llama_vocab *vocab = m_api->modelGetVocab(m_api->model);
    auto generate = [&](const QString &instructionText, int outputLimit,
                        QByteArray &translatedUtf8) {
        const QByteArray instruction = instructionText.toUtf8();
        QByteArray prompt = instruction;
        bool addSpecialTokens = true;
        const char *chatTemplate = m_api->modelChatTemplate(m_api->model, nullptr);
        if (chatTemplate) {
            const llama_chat_message message{"user", instruction.constData()};
            const int32_t needed = m_api->chatApplyTemplate(chatTemplate, &message, 1, true, nullptr, 0);
            if (needed > 0) {
                prompt.resize(needed + 1);
                const int32_t written = m_api->chatApplyTemplate(
                    chatTemplate, &message, 1, true, prompt.data(), prompt.size());
                if (written > 0) {
                    prompt.resize(written);
                    // Hy-MT2's chat template already starts with its BOS token.
                    addSpecialTokens = false;
                } else {
                    prompt = instruction;
                }
            }
        }

        int32_t tokenCount = -m_api->tokenize(vocab, prompt.constData(), prompt.size(),
                                               nullptr, 0, addSpecialTokens, true);
        if (tokenCount <= 0) return false;
        std::vector<llama_token> tokens(static_cast<size_t>(tokenCount));
        tokenCount = m_api->tokenize(vocab, prompt.constData(), prompt.size(), tokens.data(),
                                     tokenCount, addSpecialTokens, true);
        if (tokenCount <= 0) return false;

        llama_context_params contextParams = m_api->contextDefaultParams();
        contextParams.n_ctx = static_cast<uint32_t>(qMax(512, tokenCount + outputLimit + 8));
        contextParams.n_batch = static_cast<uint32_t>(qMax(512, tokenCount));
        contextParams.n_threads = qMax(1, QThread::idealThreadCount());
        contextParams.n_threads_batch = contextParams.n_threads;
        llama_context *context = m_api->contextInit(m_api->model, contextParams);
        if (!context) return false;

        llama_sampler *sampler = m_api->samplerChainInit(m_api->samplerDefaultParams());
        m_api->samplerChainAdd(sampler, m_api->samplerPenalties(-1, 1.05f, 0.0f, 0.0f));
        m_api->samplerChainAdd(sampler, m_api->samplerTopK(20));
        m_api->samplerChainAdd(sampler, m_api->samplerTopP(0.6f, 1));
        m_api->samplerChainAdd(sampler, m_api->samplerTemp(0.7f));
        m_api->samplerChainAdd(sampler, m_api->samplerDist(LLAMA_DEFAULT_SEED));

        llama_batch batch = m_api->batchGetOne(tokens.data(), tokenCount);
        bool failed = false;
        for (int generated = 0; generated < outputLimit; ++generated) {
            if (m_cancelled.load(std::memory_order_relaxed) ||
                (cancelToken && cancelToken->load(std::memory_order_relaxed))) break;
            if (m_api->decode(context, batch) != 0) { failed = true; break; }
            llama_token token = m_api->samplerSample(sampler, context, -1);
            if (m_api->vocabIsEog(vocab, token)) break;
            translatedUtf8 += tokenPiece(vocab, token, m_api->tokenToPiece);
            batch = m_api->batchGetOne(&token, 1);
        }
        m_api->samplerFree(sampler);
        m_api->contextFree(context);
        return !failed;
    };

    constexpr int maxSegmentsPerChunk = 20;
    constexpr int maxSourceCharactersPerChunk = 6000;
    int cursor = 0;
    while (cursor < texts.size()) {
        if (m_cancelled.load(std::memory_order_relaxed) ||
            (cancelToken && cancelToken->load(std::memory_order_relaxed))) {
            setError(QStringLiteral("Translation was cancelled."), error);
            return {};
        }

        QStringList chunk;
        int sourceCharacters = 0;
        while (cursor + chunk.size() < texts.size()
               && chunk.size() < maxSegmentsPerChunk) {
            const QString candidate = texts.at(cursor + chunk.size());
            if (!chunk.isEmpty()
                && sourceCharacters + candidate.size() > maxSourceCharactersPerChunk)
                break;
            chunk.append(candidate);
            sourceCharacters += candidate.size();
        }

        QString markedSource;
        for (int i = 0; i < chunk.size(); ++i) {
            if (!markedSource.isEmpty()) markedSource += QStringLiteral("\n\n");
            markedSource += segmentMarker(i + 1) + QStringLiteral("\n") + chunk.at(i);
        }
        const QString batchInstruction = QStringLiteral(
            "Translate the following ordered subtitle segments from %1 into %2. "
            "Use the surrounding segments as context so terminology, references, ranking, and "
            "style remain consistent. Preserve every marker exactly and output exactly one "
            "translation after each marker, in the same order. Do not merge, split, omit, or "
            "explain any segment.\n\n%3")
            .arg(sourceName, targetName, markedSource);
        const int outputLimit = qMin(qMax(1, maxTokens),
                                     qMax(96, sourceCharacters * 2 + chunk.size() * 20));

        QByteArray translatedUtf8;
        if (!generate(batchInstruction, outputLimit, translatedUtf8)) {
            setError(QStringLiteral("llama.cpp failed while generating the translation."), error);
            return {};
        }
        if (m_cancelled.load(std::memory_order_relaxed) ||
            (cancelToken && cancelToken->load(std::memory_order_relaxed))) {
            setError(QStringLiteral("Translation was cancelled."), error);
            return {};
        }
        QStringList chunkTranslations;
        const QString structuredOutput = QString::fromUtf8(translatedUtf8).trimmed();
        if (!parseContextTranslation(structuredOutput, chunk.size(), &chunkTranslations)) {
            Logger::warning(QStringLiteral("LlamaTranslation"),
                            QStringLiteral("Context translation returned invalid markers; retrying %1 segment(s) individually.")
                                .arg(chunk.size()));
            for (const QString &text : std::as_const(chunk)) {
                const QString fallbackInstruction = QStringLiteral(
                    "Translate the following text into %1. Note that you should only output the "
                    "translated result without any additional explanation:\n\n%2")
                    .arg(targetName, text);
                translatedUtf8.clear();
                const int fallbackLimit = qMin(qMax(1, maxTokens),
                                               qMax(32, text.size() * 2 + 24));
                if (!generate(fallbackInstruction, fallbackLimit, translatedUtf8)) {
                    setError(QStringLiteral("llama.cpp failed while retrying the translation."), error);
                    return {};
                }
                const QString translated = QString::fromUtf8(translatedUtf8).trimmed();
                if (translated.isEmpty()) {
                    setError(QStringLiteral("Translation returned an empty segment."), error);
                    return {};
                }
                chunkTranslations.append(translated);
            }
        }
        results.append(chunkTranslations);
        cursor += chunk.size();
    }
    return results;
}

} // namespace LAStudio
