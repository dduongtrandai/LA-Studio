#pragma once

#include <QString>
#include <QVariantList>

namespace LAStudio {

class ModelManager;
class RuntimeManager;
struct SessionConfiguration;

struct TranslationRequest
{
    QString modelPath;
    QString backend;
    QString runtimePath;
    QString sourceLanguage;
    QString targetLanguage;
    bool useGpu = false;
    int maxTokens = 256;
};

// Shared local text translation adapter used by Translation Studio and Dubbing.
class TranslationService final
{
public:
    TranslationService(ModelManager *models, RuntimeManager *runtimes);

    bool prepareFallback(const QString &sourceLanguage, const QString &targetLanguage,
                         TranslationRequest &request, QString *error = nullptr) const;
    static bool prepareConfiguration(const SessionConfiguration &configuration,
                                     const QString &sourceLanguage, const QString &targetLanguage,
                                     TranslationRequest &request, QString *error = nullptr);
    static bool translate(const TranslationRequest &request, const QVariantList &segments,
                          QVariantList &patches, QString *error = nullptr);

private:
    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;
};

} // namespace LAStudio
