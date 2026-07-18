#pragma once

#include "translation/TranslationService.h"

namespace LAStudio {

class ModelManager;
class RuntimeManager;

using DubbingTranslationRequest = TranslationRequest;

class DubbingTranslationService final
{
public:
    DubbingTranslationService(ModelManager *models, RuntimeManager *runtimes) : m_service(models, runtimes) {}
    bool prepare(const QString &sourceLanguage, const QString &targetLanguage,
                 DubbingTranslationRequest &request, QString *error = nullptr) const;

private:
    TranslationService m_service;
};

} // namespace LAStudio
