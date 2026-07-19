#pragma once

#include "inference/InferenceCancellation.h"

#include <QVariantList>
#include <QString>
#include <QMetaType>
#include <functional>

namespace LAStudio {

struct TranslationBackendConfiguration
{
    QString modelPath;
    QString runtimePath;
    QString backendId;
    bool useGpu = false;
    int threads = 0;
};

struct TranslationInferenceRequest
{
    QVariantList segments;
    QString sourceLanguage;
    QString targetLanguage;
    QString task = QStringLiteral("translate");
    int maxTokens = 4096;
    InferenceCancellationToken cancellation;
};

using TranslationProgressCallback = std::function<void(int percent)>;

class TranslationBackend
{
public:
    virtual ~TranslationBackend() = default;

    virtual bool loadModel(const TranslationBackendConfiguration &configuration,
                           QString &error) = 0;
    virtual void unloadModel() = 0;
    virtual void cancelProcessing() {}
    virtual bool isLoaded() const = 0;
    virtual bool translate(const TranslationInferenceRequest &request,
                           QVariantList &patches,
                           TranslationProgressCallback progress,
                           QString &error) = 0;
};

} // namespace LAStudio

Q_DECLARE_METATYPE(LAStudio::TranslationBackendConfiguration)
Q_DECLARE_METATYPE(LAStudio::TranslationInferenceRequest)
