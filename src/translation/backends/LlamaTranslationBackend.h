#pragma once

#include "TranslationBackend.h"
#include "runtimes/LlamaTranslationInterface.h"

namespace LAStudio {

class LlamaTranslationBackend final : public TranslationBackend
{
public:
    bool loadModel(const TranslationBackendConfiguration &configuration, QString &error) override;
    void unloadModel() override;
    void cancelProcessing() override { m_runtime.cancel(); }
    bool isLoaded() const override { return m_runtime.isLoaded(); }
    bool translate(const TranslationInferenceRequest &request,
                   QVariantList &patches,
                   TranslationProgressCallback progress,
                   QString &error) override;

private:
    LlamaTranslationInterface m_runtime;
    TranslationBackendConfiguration m_configuration;
};

} // namespace LAStudio
