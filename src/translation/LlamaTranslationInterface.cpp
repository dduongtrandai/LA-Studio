#include "runtimes/LlamaTranslationInterface.h"

#include "core/PathUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QLocale>
#include <QThread>
#include <QVariantMap>

#include <algorithm>
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

QStringList LlamaTranslationInterface::translateBatch(
    const QStringList &texts, const QString &sourceLanguage, const QString &targetLanguage,
    int maxTokens, const std::shared_ptr<std::atomic_bool> &cancelToken, QString *error,
    const QString &task, const QVariantList &segments)
{
    if (!isLoaded() || texts.isEmpty()) {
        setError(QStringLiteral("The llama.cpp DLL runtime is not loaded."), error);
        return {};
    }

    const QString targetName = fullLanguageName(targetLanguage);
    Q_UNUSED(sourceLanguage);

    QStringList results;
    const llama_vocab *vocab = m_api->modelGetVocab(m_api->model);
    for (const QString &text : texts) {
        if (m_cancelled.load(std::memory_order_relaxed) ||
            (cancelToken && cancelToken->load(std::memory_order_relaxed))) {
            setError(QStringLiteral("Translation was cancelled."), error);
            return {};
        }

        QString userInstruction;
        const QVariantMap segmentContext = (segments.size() == texts.size())
            ? segments.at(results.size()).toMap() : QVariantMap();
        if (task == QStringLiteral("duration-translate")) {
            userInstruction = QStringLiteral(
                "Translate the following text into %1. Preserve the complete meaning, names, "
                "numbers, order/rank, comparison, causality, negation, and every protected token. "
                "The result must fit the stated target-language phoneme budget. Follow the "
                "constraint closely, but do not count or print phonemes yourself: an external "
                "counter will verify the result. Use concise, natural wording. Output only the "
                "translated result with no explanation.\nConstraint: %2\nProtected tokens: %3\n\n%4")
                .arg(targetName,
                     segmentContext.value(QStringLiteral("durationPrompt")).toString(),
                     segmentContext.value(QStringLiteral("protectedTokens")).toString(),
                     text);
        } else {
            userInstruction = QStringLiteral(
                "Translate the following text into %1. Note that you should only output the "
                "translated result without any additional explanation:\n\n%2")
                .arg(targetName, text);
        }
        const QByteArray instruction = userInstruction.toUtf8();
        QByteArray prompt = instruction;
        const char *chatTemplate = m_api->modelChatTemplate(m_api->model, nullptr);
        if (chatTemplate) {
            const llama_chat_message message{"user", instruction.constData()};
            const int32_t needed = m_api->chatApplyTemplate(chatTemplate, &message, 1, true, nullptr, 0);
            if (needed > 0) {
                prompt.resize(needed + 1);
                const int32_t written = m_api->chatApplyTemplate(chatTemplate, &message, 1, true,
                                                                  prompt.data(), prompt.size());
                if (written > 0) prompt.resize(written);
                else prompt = instruction;
            }
        }

        int32_t tokenCount = -m_api->tokenize(vocab, prompt.constData(), prompt.size(), nullptr, 0, true, true);
        if (tokenCount <= 0) {
            setError(QStringLiteral("llama.cpp failed to tokenize the translation prompt."), error);
            return {};
        }
        std::vector<llama_token> tokens(static_cast<size_t>(tokenCount));
        tokenCount = m_api->tokenize(vocab, prompt.constData(), prompt.size(), tokens.data(), tokenCount, true, true);
        if (tokenCount <= 0) {
            setError(QStringLiteral("llama.cpp failed to tokenize the translation prompt."), error);
            return {};
        }

        int requestedLimit = qMax(32, text.size() * 2 + 24);
        if (task == QStringLiteral("duration-translate")) {
            const int targetPhonemes = segmentContext.value(QStringLiteral("durationBudget"))
                                            .toMap().value(QStringLiteral("targetUnits")).toInt();
            requestedLimit = qMax(24, targetPhonemes * 2 + 16);
        }
        const int outputLimit = qMin(qMax(1, maxTokens), requestedLimit);
        llama_context_params contextParams = m_api->contextDefaultParams();
        contextParams.n_ctx = static_cast<uint32_t>(qMax(512, tokenCount + outputLimit + 8));
        contextParams.n_batch = static_cast<uint32_t>(qMax(512, tokenCount));
        contextParams.n_threads = qMax(1, QThread::idealThreadCount());
        contextParams.n_threads_batch = contextParams.n_threads;
        llama_context *context = m_api->contextInit(m_api->model, contextParams);
        if (!context) {
            setError(QStringLiteral("llama.cpp failed to create a translation context."), error);
            return {};
        }

        llama_sampler *sampler = m_api->samplerChainInit(m_api->samplerDefaultParams());
        m_api->samplerChainAdd(sampler, m_api->samplerPenalties(-1, 1.05f, 0.0f, 0.0f));
        m_api->samplerChainAdd(sampler, m_api->samplerTopK(20));
        m_api->samplerChainAdd(sampler, m_api->samplerTopP(0.6f, 1));
        m_api->samplerChainAdd(sampler, m_api->samplerTemp(0.7f));
        m_api->samplerChainAdd(sampler, m_api->samplerDist(LLAMA_DEFAULT_SEED));

        llama_batch batch = m_api->batchGetOne(tokens.data(), tokenCount);
        QByteArray translatedUtf8;
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
        if (failed) {
            setError(QStringLiteral("llama.cpp failed while decoding the translation."), error);
            return {};
        }
        if (m_cancelled.load(std::memory_order_relaxed) ||
            (cancelToken && cancelToken->load(std::memory_order_relaxed))) {
            setError(QStringLiteral("Translation was cancelled."), error);
            return {};
        }
        results.append(QString::fromUtf8(translatedUtf8).trimmed());
    }
    return results;
}

} // namespace LAStudio
