#pragma once

#include "RuntimeHostClient.h"
#include "RuntimeHostManager.h"
#include "stt/backends/SttBackend.h"

namespace LAStudio {

class HostedWhisperBackend final : public SttBackend {
public:
    ~HostedWhisperBackend() override;
    bool loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath, QString &error) override;
    void unloadModel() override;
    void cancelProcessing() override;
    bool transcribe(const QVector<float> &samples,
                    const QString &language,
                    int threads,
                    bool translate,
                    const QVariantMap &settings,
                    QString &fullText,
                    QVariantList &segments,
                    QString &error) override;

private:
    RuntimeHostClient m_client;
    QString m_modelPath;
    bool m_gpuPermit = false;
    QString m_runtimeFamily = QStringLiteral("whisper");
};

} // namespace LAStudio
