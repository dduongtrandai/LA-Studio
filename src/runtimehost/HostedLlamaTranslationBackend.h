#pragma once

#include "RuntimeHostClient.h"
#include "RuntimeHostManager.h"
#include "translation/backends/TranslationBackend.h"

namespace LAStudio {

class HostedLlamaTranslationBackend final : public TranslationBackend {
public:
    ~HostedLlamaTranslationBackend() override;
    bool loadModel(const TranslationBackendConfiguration &configuration, QString &error) override;
    void unloadModel() override;
    void cancelProcessing() override;
    bool isLoaded() const override { return m_loaded; }
    bool translate(const TranslationInferenceRequest &request,
                   QVariantList &patches,
                   TranslationProgressCallback progress,
                   QString &error) override;

private:
    RuntimeHostClient m_client;
    bool m_loaded = false;
    bool m_gpuPermit = false;
    QString m_runtimeFamily = QStringLiteral("llama");
};

} // namespace LAStudio
