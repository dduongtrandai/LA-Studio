#pragma once

#include "backends/TranslationBackendFactory.h"

#include <QObject>
#include <memory>

namespace LAStudio {

class TranslationWorker final : public QObject
{
    Q_OBJECT
public:
    explicit TranslationWorker(std::shared_ptr<TranslationBackendFactory> factory,
                               QObject *parent = nullptr);

public slots:
    void loadModel(const TranslationBackendConfiguration &configuration);
    void unloadModel();
    void translate(const TranslationInferenceRequest &request);
    void cancelProcessing();

signals:
    void modelLoaded(bool success, const QString &error);
    void unloaded();
    void progress(int percent);
    void finished(const QVariantList &patches);
    void errorOccurred(const QString &error);

private:
    std::shared_ptr<TranslationBackendFactory> m_factory;
    std::unique_ptr<TranslationBackend> m_backend;
};

} // namespace LAStudio
