#include "HostedLlamaTranslationBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace LAStudio {

HostedLlamaTranslationBackend::~HostedLlamaTranslationBackend()
{
    unloadModel();
}

bool HostedLlamaTranslationBackend::loadModel(const TranslationBackendConfiguration &configuration,
                                              QString &error)
{
    unloadModel();
    const bool gpu = configuration.useGpu
                     || configuration.runtimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive);
    if (!RuntimeHostManager::instance().acquire(m_runtimeFamily, gpu, &error)) return false;
    m_gpuPermit = gpu;
    const QString hostPath = QDir(QCoreApplication::applicationDirPath())
                                 .absoluteFilePath(QStringLiteral("LAStudioRuntimeHost.exe"));
    if (!QFileInfo(hostPath).isFile()) {
        error = QStringLiteral("LAStudioRuntimeHost.exe is missing: %1").arg(hostPath);
        RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
        m_gpuPermit = false;
        return false;
    }
    if (!m_client.start(hostPath, &error)) {
        RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
        m_gpuPermit = false;
        return false;
    }
    const QCborMap config{
        {QStringLiteral("adapter"), QStringLiteral("llama")},
        {QStringLiteral("model"), configuration.modelPath},
        {QStringLiteral("runtimePath"), configuration.runtimePath},
        {QStringLiteral("useGpu"), configuration.useGpu},
        {QStringLiteral("threads"), configuration.threads}
    };
    QCborValue schema;
    if (!m_client.load(config, &schema, &error)) {
        m_client.shutdown();
        RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
        m_gpuPermit = false;
        return false;
    }
    m_loaded = true;
    return true;
}

void HostedLlamaTranslationBackend::unloadModel()
{
    QString ignored;
    m_client.shutdown(&ignored);
    RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
    m_gpuPermit = false;
    m_loaded = false;
}

void HostedLlamaTranslationBackend::cancelProcessing()
{
    m_client.cancelCurrent();
}

bool HostedLlamaTranslationBackend::translate(const TranslationInferenceRequest &request,
                                              QVariantList &patches,
                                              TranslationProgressCallback progress,
                                              QString &error)
{
    if (!m_loaded) {
        error = QStringLiteral("Llama runtime is not loaded.");
        return false;
    }
    const QCborMap payload{
        {QStringLiteral("mode"), QStringLiteral("translate")},
        {QStringLiteral("segments"), QCborValue::fromVariant(request.segments)},
        {QStringLiteral("sourceLanguage"), request.sourceLanguage},
        {QStringLiteral("targetLanguage"), request.targetLanguage},
        {QStringLiteral("task"), request.task},
        {QStringLiteral("maxTokens"), request.maxTokens}
    };
    QCborMap result;
    if (!m_client.execute(payload, {}, &result, nullptr, nullptr, &error)) return false;
    patches = result.value(QStringLiteral("patches")).toVariant().toList();
    if (progress) progress(100);
    return true;
}

} // namespace LAStudio
