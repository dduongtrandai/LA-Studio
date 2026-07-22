#include "RuntimeHostAdapter.h"

#include "tts/backends/OmnivoiceBackend.h"
#include "stt/backends/WhisperSttBackend.h"
#include "translation/backends/LlamaTranslationBackend.h"
#include "inference/InferenceCancellation.h"

#include <QCborValue>
#include <QCborArray>
#include <QVariantMap>

#include <memory>

namespace LAStudio {

bool RuntimeHostAdapter::execute(const QCborMap &request,
                                 const QVector<float> &referenceSamples,
                                 Result *result,
                                 QString *error)
{
    if (!result) {
        if (error) *error = QStringLiteral("RuntimeHost result destination is null.");
        return false;
    }
    result->payload.clear();
    result->samples.clear();
    result->sampleRate = 0;
    return infer(request, referenceSamples, &result->samples, &result->sampleRate, error);
}

namespace {

class OmnivoiceHostAdapter final : public RuntimeHostAdapter {
public:
    bool load(const QCborMap &configuration, QCborValue *schema, QString *error) override
    {
        QVariantMap config = configuration.toVariantMap();
        config.insert(QStringLiteral("backend"), QStringLiteral("omnivoice"));
        QVariantList schemaList;
        if (!m_backend.load(config, m_error, schemaList)) {
            if (error) *error = m_error;
            return false;
        }
        if (schema) *schema = QCborValue::fromVariant(schemaList);
        return true;
    }

    void unload() override
    {
        m_backend.unload();
    }

    bool infer(const QCborMap &request,
               const QVector<float> &referenceSamples,
               QVector<float> *samples,
               int *sampleRate,
               QString *error) override
    {
        if (!samples || !sampleRate) {
            if (error) *error = QStringLiteral("RuntimeHost output destination is null.");
            return false;
        }
        const QString text = request.value(QStringLiteral("text")).toString();
        const float speed = static_cast<float>(request.value(QStringLiteral("speed")).toDouble(1.0));
        const QVariantMap settings = request.value(QStringLiteral("settings")).toMap().toVariantMap();
        const QString mode = request.value(QStringLiteral("mode")).toString();
        QString localError;
        bool ok = false;
        if (mode == QStringLiteral("clone")) {
            ok = m_backend.cloneVoiceWithReferenceSamples(text, referenceSamples, settings,
                                                            *samples, *sampleRate, localError);
        } else {
            ok = m_backend.synthesize(text, speed, settings, *samples, *sampleRate, localError);
        }
        if (!ok && error) *error = localError;
        return ok;
    }

    void cancel() override { m_backend.cancelProcessing(); }

    void setProgressCallback(ProgressCallback callback) override
    {
        m_backend.setProgressCallback(std::move(callback));
    }

private:
    OmnivoiceBackend m_backend;
    QString m_error;
};

class WhisperHostAdapter final : public RuntimeHostAdapter {
public:
    bool load(const QCborMap &configuration, QCborValue *schema, QString *error) override
    {
        const QVariantMap config = configuration.toVariantMap();
        const QString modelPath = config.value(QStringLiteral("model")).toString();
        const QString runtimePath = config.value(QStringLiteral("runtimePath")).toString();
        const bool useGpu = config.value(QStringLiteral("useGpu")).toBool();
        QString localError;
        if (!m_backend.loadModel(modelPath, useGpu, runtimePath, localError)) {
            if (error) *error = localError;
            return false;
        }
        if (schema) *schema = QCborValue::fromVariant(QVariantList{
            QVariantMap{{QStringLiteral("name"), QStringLiteral("fullText")},
                        {QStringLiteral("type"), QStringLiteral("string")}},
            QVariantMap{{QStringLiteral("name"), QStringLiteral("segments")},
                        {QStringLiteral("type"), QStringLiteral("array")}}
        });
        return true;
    }

    void unload() override { m_backend.unloadModel(); }

    bool infer(const QCborMap &request,
               const QVector<float> &referenceSamples,
               QVector<float> *samples,
               int *sampleRate,
               QString *error) override
    {
        Q_UNUSED(samples);
        Q_UNUSED(sampleRate);
        Q_UNUSED(request);
        Q_UNUSED(referenceSamples);
        if (error) *error = QStringLiteral("Whisper adapter requires structured execution.");
        return false;
    }

    bool execute(const QCborMap &request,
                 const QVector<float> &inputSamples,
                 Result *result,
                 QString *error) override
    {
        if (!result) {
            if (error) *error = QStringLiteral("RuntimeHost result destination is null.");
            return false;
        }
        const QString language = request.value(QStringLiteral("language")).toString();
        const int threads = request.value(QStringLiteral("threads")).toInteger(0);
        const bool translate = request.value(QStringLiteral("translate")).toBool(false);
        const QVariantMap settings = request.value(QStringLiteral("settings")).toMap().toVariantMap();
        QString fullText;
        QVariantList segments;
        QString localError;
        if (!m_backend.transcribe(inputSamples, language, threads, translate, settings,
                                  fullText, segments, localError)) {
            if (error) *error = localError;
            return false;
        }
        result->payload = QCborMap{
            {QStringLiteral("fullText"), fullText},
            {QStringLiteral("segments"), QCborValue::fromVariant(segments)}
        };
        return true;
    }

    void cancel() override { m_backend.cancelProcessing(); }
    void setProgressCallback(ProgressCallback) override {}

private:
    WhisperSttBackend m_backend;
};

class LlamaHostAdapter final : public RuntimeHostAdapter {
public:
    bool load(const QCborMap &configuration, QCborValue *schema, QString *error) override
    {
        const QVariantMap config = configuration.toVariantMap();
        TranslationBackendConfiguration backendConfig;
        backendConfig.modelPath = config.value(QStringLiteral("model")).toString();
        backendConfig.runtimePath = config.value(QStringLiteral("runtimePath")).toString();
        backendConfig.backendId = QStringLiteral("llama");
        backendConfig.useGpu = config.value(QStringLiteral("useGpu")).toBool();
        backendConfig.threads = config.value(QStringLiteral("threads")).toInt();
        if (!m_backend.loadModel(backendConfig, *error)) return false;
        if (schema) *schema = QCborValue::fromVariant(QVariantList{
            QVariantMap{{QStringLiteral("name"), QStringLiteral("patches")},
                        {QStringLiteral("type"), QStringLiteral("array")}}
        });
        return true;
    }

    void unload() override { m_backend.unloadModel(); }

    bool infer(const QCborMap &, const QVector<float> &, QVector<float> *, int *, QString *error) override
    {
        if (error) *error = QStringLiteral("Llama adapter requires structured execution.");
        return false;
    }

    bool execute(const QCborMap &request, const QVector<float> &, Result *result,
                 QString *error) override
    {
        if (!result) {
            if (error) *error = QStringLiteral("RuntimeHost result destination is null.");
            return false;
        }
        TranslationInferenceRequest translationRequest;
        m_cancelToken = InferenceCancellationToken();
        translationRequest.segments = request.value(QStringLiteral("segments")).toArray().toVariantList();
        translationRequest.sourceLanguage = request.value(QStringLiteral("sourceLanguage")).toString();
        translationRequest.targetLanguage = request.value(QStringLiteral("targetLanguage")).toString();
        translationRequest.task = request.value(QStringLiteral("task")).toString(QStringLiteral("translate"));
        translationRequest.maxTokens = request.value(QStringLiteral("maxTokens")).toInteger(4096);
        translationRequest.cancellation = m_cancelToken;
        QVariantList patches;
        if (!m_backend.translate(translationRequest, patches, {}, *error)) return false;
        result->payload = QCborMap{{QStringLiteral("patches"), QCborValue::fromVariant(patches)}};
        return true;
    }

    void cancel() override
    {
        m_cancelToken.cancel();
        m_backend.cancelProcessing();
    }
    void setProgressCallback(ProgressCallback) override {}

private:
    LlamaTranslationBackend m_backend;
    InferenceCancellationToken m_cancelToken;
};

class LlamaChatHostAdapter final : public RuntimeHostAdapter {
public:
    bool load(const QCborMap &configuration, QCborValue *schema, QString *error) override
    {
        const QVariantMap config = configuration.toVariantMap();
        if (!m_runtime.load(config.value(QStringLiteral("runtimePath")).toString(),
                            config.value(QStringLiteral("model")).toString(),
                            error, config.value(QStringLiteral("useGpu")).toBool())) {
            return false;
        }
        if (schema) *schema = QCborValue::fromVariant(QVariantList{
            QVariantMap{{QStringLiteral("name"), QStringLiteral("fullText")},
                        {QStringLiteral("type"), QStringLiteral("string")}}
        });
        return true;
    }

    void unload() override { m_runtime.unload(); }

    bool infer(const QCborMap &, const QVector<float> &, QVector<float> *, int *, QString *error) override
    {
        if (error) *error = QStringLiteral("Llama chat adapter requires structured execution.");
        return false;
    }

    bool execute(const QCborMap &request, const QVector<float> &, Result *result,
                 QString *error) override
    {
        if (!result) {
            if (error) *error = QStringLiteral("RuntimeHost result destination is null.");
            return false;
        }
        const QVariantList rawMessages = request.value(QStringLiteral("messages"))
                                             .toArray().toVariantList();
        QList<QVariantMap> typedMessages;
        for (const QVariant &message : rawMessages) typedMessages.append(message.toMap());
        const int contextTokens = request.value(QStringLiteral("contextTokens")).toInteger(4096);
        const int maxTokens = request.value(QStringLiteral("maxTokens")).toInteger(512);
        const float temperature = static_cast<float>(request.value(QStringLiteral("temperature")).toDouble(0.7));
        const float topP = static_cast<float>(request.value(QStringLiteral("topP")).toDouble(0.9));
        const int topK = request.value(QStringLiteral("topK")).toInteger(40);
        const float repeatPenalty = static_cast<float>(request.value(QStringLiteral("repeatPenalty")).toDouble(1.1));
        m_cancelToken = std::make_shared<std::atomic_bool>(false);
        QString fullText;
        const bool ok = m_runtime.generateChat(
            typedMessages, contextTokens, maxTokens, temperature, topP, topK, repeatPenalty,
            m_cancelToken,
            [this](const QString &token) {
                if (m_progressCallback) m_progressCallback(0, 0, token, 0, 0);
            },
            &fullText, error);
        const bool cancelled = m_cancelToken->load(std::memory_order_relaxed);
        m_cancelToken.reset();
        if (!ok) return false;
        if (cancelled) {
            if (error) *error = QStringLiteral("Generation cancelled.");
            return false;
        }
        result->payload = QCborMap{{QStringLiteral("fullText"), fullText}};
        return true;
    }

    void cancel() override
    {
        if (m_cancelToken) m_cancelToken->store(true, std::memory_order_relaxed);
        m_runtime.cancel();
    }

    void setProgressCallback(ProgressCallback callback) override
    {
        m_progressCallback = std::move(callback);
    }

private:
    LlamaTranslationInterface m_runtime;
    std::shared_ptr<std::atomic_bool> m_cancelToken;
    ProgressCallback m_progressCallback;
};

} // namespace

std::unique_ptr<RuntimeHostAdapter> createRuntimeHostAdapter(const QString &adapterId)
{
    if (adapterId.compare(QStringLiteral("omnivoice"), Qt::CaseInsensitive) == 0) {
        return std::make_unique<OmnivoiceHostAdapter>();
    }
    if (adapterId.compare(QStringLiteral("whisper"), Qt::CaseInsensitive) == 0) {
        return std::make_unique<WhisperHostAdapter>();
    }
    if (adapterId.compare(QStringLiteral("llama"), Qt::CaseInsensitive) == 0) {
        return std::make_unique<LlamaHostAdapter>();
    }
    if (adapterId.compare(QStringLiteral("llama-chat"), Qt::CaseInsensitive) == 0) {
        return std::make_unique<LlamaChatHostAdapter>();
    }
    return nullptr;
}

} // namespace LAStudio
