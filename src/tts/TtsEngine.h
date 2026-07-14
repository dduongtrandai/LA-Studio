#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <QHash>
#include <QtQml/qqml.h>
#include "controllers/IModelSession.h"
#include "TtsEngineInstance.h"

namespace LAStudio {

class TtsEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TtsEngine is managed by AppController")

    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(QByteArray lastPcm READ lastPcm NOTIFY synthesisFinished)
    Q_PROPERTY(QVector<float> lastSamples READ lastSamples NOTIFY synthesisFinished)
    Q_PROPERTY(QVariantList lastSamplePreview READ lastSamplePreview NOTIFY synthesisFinished)
    Q_PROPERTY(int lastSampleCount READ lastSampleCount NOTIFY synthesisFinished)
    Q_PROPERTY(QVariantList currentSchema READ currentSchema NOTIFY schemaChanged)
    Q_PROPERTY(QVariantMap familyConfig READ familyConfig WRITE setFamilyConfig NOTIFY familyConfigChanged)
    Q_PROPERTY(qint64 estimatedRamBytes READ estimatedRamBytes NOTIFY memoryUsageChanged)
    Q_PROPERTY(qint64 estimatedVramBytes READ estimatedVramBytes NOTIFY memoryUsageChanged)
    Q_PROPERTY(QString estimatedRamUsage READ estimatedRamUsage NOTIFY memoryUsageChanged)
    Q_PROPERTY(QString estimatedVramUsage READ estimatedVramUsage NOTIFY memoryUsageChanged)
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool isCloneAction READ isCloneAction NOTIFY synthesisFinished)
    Q_PROPERTY(QString lastGenerationMode READ lastGenerationMode NOTIFY lastGenerationModeChanged)
    Q_PROPERTY(int generationProgress READ generationProgress NOTIFY generationProgressChanged)
    Q_PROPERTY(bool generationProgressEstimated READ generationProgressEstimated NOTIFY generationProgressChanged)
    Q_PROPERTY(QString generationProgressLabel READ generationProgressLabel NOTIFY generationProgressChanged)
    Q_PROPERTY(QString activeSignature READ activeSignature NOTIFY activeSignatureChanged)

public:
    enum State {
        Unloaded = TtsEngineInstance::Unloaded,
        Loading = TtsEngineInstance::Loading,
        Ready = TtsEngineInstance::Ready,
        Processing = TtsEngineInstance::Processing,
        Error = TtsEngineInstance::Error
    };
    Q_ENUM(State)

    explicit TtsEngine(QObject *parent = nullptr);
    ~TtsEngine() override;

    State state() const;
    bool isModelLoaded() const;
    bool isProcessing() const;
    int sampleRate() const;
    QVariantList currentSchema() const;
    QVariantMap familyConfig() const;
    void setFamilyConfig(const QVariantMap &config);
    qint64 estimatedRamBytes() const;
    qint64 estimatedVramBytes() const;
    QString estimatedRamUsage() const;
    QString estimatedVramUsage() const;
    double cpuUsage() const;
    bool isCloneAction() const;
    QString lastGenerationMode() const;
    int generationProgress() const;
    bool generationProgressEstimated() const;
    QString generationProgressLabel() const;
    QString activeSignature() const { return m_activeSignature; }

    Q_INVOKABLE QVariantList schemaForCapability(const QString &capability) const;
    Q_INVOKABLE QVariantMap studioConfigForCapability(const QString &capability) const;

    Q_INVOKABLE void loadVoice(const QVariantMap &config);
    Q_INVOKABLE void loadModel(const QString &modelPath,
                               const QString &codecPath = QString(),
                               const QString &runtimePath = QString());
    Q_INVOKABLE void loadModel(const QVariantMap &filesByRole,
                               const QString &runtimePath = QString());
    Q_INVOKABLE void setRuntimePath(const QString &runtimePath);
    Q_INVOKABLE void unloadVoice();
    Q_INVOKABLE void unloadVoiceSync();
    Q_INVOKABLE void clearLastSamples();
    Q_INVOKABLE void synthesize(const QString &text, int speakerId = 0,
                                float speed = 1.0f, const QVariantMap &settings = QVariantMap());
    Q_INVOKABLE void cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings = QVariantMap());
    Q_INVOKABLE void designVoice(const QString &text, const QVariantMap &settings = QVariantMap());
    Q_INVOKABLE void cancelProcessing();

    QByteArray lastPcm() const;
    QVector<float> lastSamples() const;
    QVariantList lastSamplePreview() const;
    int lastSampleCount() const;

    TtsEngineInstance *loadInstance(const SessionConfiguration &config);
    bool activateInstance(const QString &signature);
    void unloadInstance(const QString &signature);
    TtsEngineInstance *instance(const QString &signature) const;
    QList<TtsEngineInstance *> loadedInstances() const;
    QStringList loadedSignatures() const;

signals:
    void modelLoadedChanged();
    void processingChanged();
    void sampleRateChanged();
    void synthesisFinished(const QByteArray &pcm16, int sampleRate);
    void errorOccurred(const QString &error);
    void schemaChanged();
    void familyConfigChanged();
    void memoryUsageChanged();
    void cpuUsageChanged();
    void stateChanged();
    void lastGenerationModeChanged();
    void generationProgressChanged();
    void activeSignatureChanged();
    void loadedInstancesChanged();

private:
    TtsEngineInstance *activeInstance() const;
    TtsEngineInstance *ensureInstance(const QString &signature);
    void connectInstance(TtsEngineInstance *inst);
    void emitActiveForwardSignals();

    QHash<QString, TtsEngineInstance*> m_instances;
    QString m_activeSignature;
    QVariantMap m_familyConfig;
    QString m_runtimePath;
};

} // namespace LAStudio
