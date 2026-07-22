#include "HostedOmnivoiceBackend.h"

#include "audio/AudioFileDecoder.h"
#include "audio/WavIO.h"
#include "core/PathUtils.h"

#include <QCoreApplication>
#include <QCborArray>
#include <QCborValue>
#include <QFileInfo>
#include <QDir>

namespace LAStudio {

bool HostedOmnivoiceBackend::load(const QVariantMap &config,
                                  QString &error,
                                  QVariantList &schema)
{
    m_config = config;
    const QString runtimePath = config.value(QStringLiteral("runtimePath")).toString();
    const bool useGpu = config.value(QStringLiteral("useGpu")).toBool()
                        || config.value(QStringLiteral("use_gpu")).toBool()
                        || runtimePath.contains(QStringLiteral("cuda"), Qt::CaseInsensitive);
    if (!RuntimeHostManager::instance().acquire(m_runtimeFamily, useGpu, &error)) return false;
    m_gpuPermit = useGpu;
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

    QCborMap hostConfig = QCborValue::fromVariant(config).toMap();
    hostConfig.insert(QStringLiteral("adapter"), QStringLiteral("omnivoice"));
    QCborValue hostSchema;
    if (!m_client.load(hostConfig, &hostSchema, &error)) {
        m_client.shutdown();
        RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
        m_gpuPermit = false;
        return false;
    }
    if (hostSchema.isArray()) schema = hostSchema.toArray().toVariantList();
    return true;
}

void HostedOmnivoiceBackend::unload()
{
    QString ignored;
    m_client.shutdown(&ignored);
    RuntimeHostManager::instance().release(m_runtimeFamily, m_gpuPermit);
    m_gpuPermit = false;
    m_config.clear();
}

void HostedOmnivoiceBackend::setProgressCallback(std::function<bool(int current,
                                                                     int total,
                                                                     const QString &stage,
                                                                     int chunkIndex,
                                                                     int chunkCount)> callback)
{
    m_progressCallback = std::move(callback);
    m_client.setProgressCallback([this](const QCborMap &payload) {
        if (!m_progressCallback) return;
        m_progressCallback(payload.value(QStringLiteral("current")).toInteger(),
                           payload.value(QStringLiteral("total")).toInteger(),
                           payload.value(QStringLiteral("stage")).toString(),
                           payload.value(QStringLiteral("chunkIndex")).toInteger(),
                           payload.value(QStringLiteral("chunkCount")).toInteger());
    });
}

void HostedOmnivoiceBackend::cancelProcessing()
{
    m_client.cancelCurrent();
}

bool HostedOmnivoiceBackend::synthesize(const QString &text,
                                        float speed,
                                        const QVariantMap &settings,
                                        QVector<float> &samples,
                                        int &sampleRate,
                                        QString &error)
{
    return infer(QStringLiteral("synthesize"), text, speed, settings, {},
                 samples, sampleRate, error);
}

bool HostedOmnivoiceBackend::cloneVoice(const QString &text,
                                        const QString &referencePath,
                                        const QVariantMap &settings,
                                        QVector<float> &samples,
                                        int &sampleRate,
                                        QString &error)
{
    QString decodeError;
    const WavIO::WavData reference = AudioFileDecoder::decodeMono(
        PathUtils::toNativeShortPath(PathUtils::urlToLocalPath(referencePath)), 24000, &decodeError);
    if (reference.samples.isEmpty()) {
        error = QStringLiteral("Failed to load reference audio: %1").arg(decodeError);
        return false;
    }
    return infer(QStringLiteral("clone"), text, 1.0f, settings, reference.samples,
                 samples, sampleRate, error);
}

bool HostedOmnivoiceBackend::infer(const QString &mode,
                                   const QString &text,
                                   float speed,
                                   const QVariantMap &settings,
                                   const QVector<float> &referenceSamples,
                                   QVector<float> &samples,
                                   int &sampleRate,
                                   QString &error)
{
    if (text.trimmed().isEmpty()) {
        error = QStringLiteral("Text input is empty.");
        return false;
    }
    const QCborMap request{{QStringLiteral("mode"), mode},
                           {QStringLiteral("text"), text},
                           {QStringLiteral("speed"), static_cast<double>(speed)},
                           {QStringLiteral("settings"), QCborValue::fromVariant(settings)}};
    return m_client.infer(request, referenceSamples, &samples, &sampleRate, &error);
}

} // namespace LAStudio
