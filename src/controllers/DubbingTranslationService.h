#pragma once

#include <QString>
#include <QVariantList>

namespace LAStudio {

class ModelManager;
class RuntimeManager;

struct DubbingTranslationRequest
{
    QString modelPath;
    QString backend;
    QString runtimePath;
    QString sourceLanguage;
    QString targetLanguage;
    bool useGpu = false;
};

class DubbingTranslationService final
{
public:
    DubbingTranslationService(ModelManager *models, RuntimeManager *runtimes);
    bool prepare(const QString &sourceLanguage, const QString &targetLanguage,
                 DubbingTranslationRequest &request, QString *error = nullptr) const;
    static bool translate(const DubbingTranslationRequest &request, const QVariantList &segments,
                          QVariantList &patches, QString *error = nullptr);

private:
    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;
};

} // namespace LAStudio
