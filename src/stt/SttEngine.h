#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QHash>
#include <QtQml/qqml.h>
#include "controllers/models/IModelSession.h"
#include "inference/InstanceRegistry.h"
#include "SttEngineInstance.h"

namespace LAStudio {

class SttEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("SttEngine is managed by AppController")

    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString transcript READ transcript NOTIFY transcriptChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString activeSignature READ activeSignature NOTIFY activeSignatureChanged)

public:
    enum State {
        Unloaded = SttEngineInstance::Unloaded,
        Loading = SttEngineInstance::Loading,
        Ready = SttEngineInstance::Ready,
        Processing = SttEngineInstance::Processing,
        Error = SttEngineInstance::Error
    };
    Q_ENUM(State)

    explicit SttEngine(QObject *parent = nullptr);
    ~SttEngine() override;

    State state() const;
    bool isModelLoaded() const;
    bool isProcessing() const;
    int progress() const;
    QString transcript() const;
    QString activeSignature() const { return m_activeSignature; }

    Q_INVOKABLE void loadModel(const QString &modelPath, bool useGpu = false, const QString &runtimePath = QString());
    Q_INVOKABLE void unloadModel();
    Q_INVOKABLE void unloadModelSync();
    Q_INVOKABLE void transcribeSamples(const QVector<float> &samples, const QString &language = "en", int threads = 0, bool translate = false, const QVariantMap &settings = QVariantMap());
    Q_INVOKABLE void cancelProcessing();

    SttEngineInstance *loadInstance(const SessionConfiguration &config, bool useGpu);
    bool activateInstance(const QString &signature);
    void unloadInstance(const QString &signature);
    SttEngineInstance *instance(const QString &signature) const;
    QList<SttEngineInstance *> loadedInstances() const;
    QStringList loadedSignatures() const;

signals:
    void modelLoadedChanged();
    void processingChanged();
    void progressChanged();
    void transcriptChanged();
    void transcriptionFinished(const QString &text, const QVariantList &segments);
    void errorOccurred(const QString &error);
    void stateChanged();
    void activeSignatureChanged();
    void loadedInstancesChanged();

private:
    SttEngineInstance *activeInstance() const;
    SttEngineInstance *ensureInstance(const QString &signature);
    void connectInstance(SttEngineInstance *inst);
    void emitActiveForwardSignals();

    InstanceRegistry<SttEngineInstance> m_instances;
    QString m_activeSignature;
};

} // namespace LAStudio
