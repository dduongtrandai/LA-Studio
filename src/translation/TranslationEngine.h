#pragma once

#include "TranslationEngineInstance.h"
#include "backends/TranslationBackendFactory.h"
#include "inference/InstanceRegistry.h"

#include <QObject>
#include <QHash>
#include <QtQml/qqml.h>
#include <memory>

namespace LAStudio {

class TranslationEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TranslationEngine is managed by AppController")
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString activeSignature READ activeSignature NOTIFY activeSignatureChanged)
public:
    enum State {
        Unloaded = TranslationEngineInstance::Unloaded,
        Loading = TranslationEngineInstance::Loading,
        Ready = TranslationEngineInstance::Ready,
        Processing = TranslationEngineInstance::Processing,
        Error = TranslationEngineInstance::Error
    };
    Q_ENUM(State)

    explicit TranslationEngine(std::shared_ptr<TranslationBackendFactory> factory = {},
                               QObject *parent = nullptr);
    ~TranslationEngine() override;

    State state() const;
    bool isModelLoaded() const;
    bool isProcessing() const;
    int progress() const;
    QString activeSignature() const { return m_activeSignature; }

    TranslationEngineInstance *loadInstance(const SessionConfiguration &configuration,
                                            bool activate = true);
    bool activateInstance(const QString &signature);
    void unloadInstance(const QString &signature);
    TranslationEngineInstance *instance(const QString &signature) const;
    QList<TranslationEngineInstance *> loadedInstances() const;
    QList<SessionConfiguration> loadedConfigurations() const;
    QStringList loadedSignatures() const;

    void translate(const TranslationInferenceRequest &request);
    void cancelProcessing();

signals:
    void modelLoadedChanged();
    void processingChanged();
    void progressChanged();
    void translationFinished(const QVariantList &patches);
    void errorOccurred(const QString &error);
    void stateChanged();
    void activeSignatureChanged();
    void loadedInstancesChanged();

private:
    TranslationEngineInstance *activeInstance() const;
    TranslationEngineInstance *ensureInstance(const SessionConfiguration &configuration);
    void connectInstance(TranslationEngineInstance *instance);
    void emitActiveForwardSignals();

    std::shared_ptr<TranslationBackendFactory> m_factory;
    InstanceRegistry<TranslationEngineInstance> m_instances;
    QString m_activeSignature;
};

} // namespace LAStudio
