#include "HostedWhisperBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace LAStudio {

HostedWhisperBackend::~HostedWhisperBackend()
{
    unloadModel();
}

bool HostedWhisperBackend::loadModel(const QString &modelPath, bool useGpu,
                                     const QString &runtimePath, QString &error)
{
    unloadModel();
    const bool gpu = useGpu || runtimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive);
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
        {QStringLiteral("adapter"), QStringLiteral("whisper")},
        {QStringLiteral("model"), modelPath},
        {QStringLiteral("runtimePath"), runtimePath},
        {QStringLiteral("useGpu"), useGpu}
    };
    QCborValue ignoredSchema;
    if (!m_client.load(config, &ignoredSchema, &error)) {
        m_client.shutdown();
        RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
        m_gpuPermit = false;
        return false;
    }
    m_modelPath = modelPath;
    return true;
}

void HostedWhisperBackend::unloadModel()
{
    QString ignored;
    m_client.shutdown(&ignored);
    RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
    m_gpuPermit = false;
    m_modelPath.clear();
}

void HostedWhisperBackend::cancelProcessing()
{
    m_client.cancelCurrent();
}

bool HostedWhisperBackend::transcribe(const QVector<float> &samples,
                                      const QString &language,
                                      int threads,
                                      bool translate,
                                      const QVariantMap &settings,
                                      QString &fullText,
                                      QVariantList &segments,
                                      QString &error)
{
    if (samples.isEmpty()) {
        error = QStringLiteral("Whisper input audio is empty.");
        return false;
    }
    const QCborMap request{
        {QStringLiteral("mode"), QStringLiteral("transcribe")},
        {QStringLiteral("language"), language},
        {QStringLiteral("threads"), threads},
        {QStringLiteral("translate"), translate},
        {QStringLiteral("settings"), QCborValue::fromVariant(settings)}
    };
    QCborMap result;
    if (!m_client.execute(request, samples, &result, nullptr, nullptr, &error, 16000)) return false;
    fullText = result.value(QStringLiteral("fullText")).toString();
    segments = result.value(QStringLiteral("segments")).toVariant().toList();
    return true;
}

} // namespace LAStudio
