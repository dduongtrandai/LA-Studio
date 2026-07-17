#pragma once

#include "TranslationBackend.h"
#include "runtimes/CrispTranslationInterface.h"

namespace LAStudio {

class CrispTranslationBackend final : public TranslationBackend
{
public:
    bool loadModel(const TranslationBackendConfiguration &configuration, QString &error) override;
    void unloadModel() override;
    bool isLoaded() const override { return m_runtime.isLoaded(); }
    bool translate(const TranslationInferenceRequest &request,
                   QVariantList &patches,
                   TranslationProgressCallback progress,
                   QString &error) override;

private:
    CrispTranslationInterface m_runtime;
    TranslationBackendConfiguration m_configuration;
};

} // namespace LAStudio
