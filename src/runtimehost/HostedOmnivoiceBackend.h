#pragma once

#include "tts/backends/TtsBackend.h"
#include "RuntimeHostClient.h"
#include "RuntimeHostManager.h"

namespace LAStudio {

// TTS-facing proxy. It preserves the existing synchronous TtsBackend contract
// while the native OmniVoice DLL lives entirely inside RuntimeHost.
class HostedOmnivoiceBackend final : public TtsBackend {
public:
    HostedOmnivoiceBackend() = default;
    ~HostedOmnivoiceBackend() override = default;

    bool load(const QVariantMap &config, QString &error, QVariantList &schema) override;
    void unload() override;
    void setProgressCallback(std::function<bool(int current,
                                                int total,
                                                const QString &stage,
                                                int chunkIndex,
                                                int chunkCount)> callback) override;
    void cancelProcessing() override;
    bool synthesize(const QString &text, float speed, const QVariantMap &settings,
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings,
                    QVector<float> &samples, int &sampleRate, QString &error) override;

private:
    bool infer(const QString &mode,
               const QString &text,
               float speed,
               const QVariantMap &settings,
               const QVector<float> &referenceSamples,
               QVector<float> &samples,
               int &sampleRate,
               QString &error);

    RuntimeHostClient m_client;
    bool m_gpuPermit = false;
    QString m_runtimeFamily = QStringLiteral("omnivoice");
    QVariantMap m_config;
    std::function<bool(int current, int total, const QString &stage, int chunkIndex, int chunkCount)> m_progressCallback;
};

} // namespace LAStudio
