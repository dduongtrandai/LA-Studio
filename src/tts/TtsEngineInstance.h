#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <QHash>
#include <QtGlobal>
#include <QtQml/qqml.h>
#include <variant>

class QTimer;

namespace LAStudio {

class TtsWorker;

class TtsEngineInstance : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TtsEngineInstance is managed by TtsEngine")

    Q_PROPERTY(bool modelLoaded READ isModelLoaded NOTIFY modelLoadedChanged)
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(int sampleRate READ sampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(QByteArray lastPcm READ lastPcm NOTIFY outputChanged)
    Q_PROPERTY(QVector<float> lastSamples READ lastSamples NOTIFY outputChanged)
    Q_PROPERTY(QVariantList lastSamplePreview READ lastSamplePreview NOTIFY outputChanged)
    Q_PROPERTY(int lastSampleCount READ lastSampleCount NOTIFY outputChanged)
    Q_PROPERTY(QVariantList currentSchema READ currentSchema NOTIFY schemaChanged)
    Q_PROPERTY(QVariantMap familyConfig READ familyConfig WRITE setFamilyConfig NOTIFY familyConfigChanged)
    Q_PROPERTY(qint64 estimatedRamBytes READ estimatedRamBytes NOTIFY memoryUsageChanged)
    Q_PROPERTY(qint64 estimatedVramBytes READ estimatedVramBytes NOTIFY memoryUsageChanged)
    Q_PROPERTY(QString estimatedRamUsage READ estimatedRamUsage NOTIFY memoryUsageChanged)
    Q_PROPERTY(QString estimatedVramUsage READ estimatedVramUsage NOTIFY memoryUsageChanged)
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool isCloneAction READ isCloneAction NOTIFY outputChanged)
    Q_PROPERTY(QString lastGenerationMode READ lastGenerationMode NOTIFY lastGenerationModeChanged)
    Q_PROPERTY(int generationProgress READ generationProgress NOTIFY generationProgressChanged)
    Q_PROPERTY(bool generationProgressEstimated READ generationProgressEstimated NOTIFY generationProgressChanged)
    Q_PROPERTY(QString generationProgressLabel READ generationProgressLabel NOTIFY generationProgressChanged)


public:
    enum State {
        Unloaded,
        Loading,
        Ready,
        Processing,
        Error
    };
    Q_ENUM(State)

    // State Machine States
    struct StateUnloaded {};
    struct StateLoading { bool cancelRequested = false; };
    struct StateReady {};
    struct StateProcessing { bool unloadRequested = false; bool stopRequested = false; };
    struct StateError { QString message; };

    using EngineState = std::variant<StateUnloaded, StateLoading, StateReady, StateProcessing, StateError>;

    // State Machine Events
    struct EventLoadVoice { QVariantMap config; };
    struct EventUnload {};
    struct EventWorkerLoaded { bool success; QString error; QVariantList schema; };
    struct EventSynthesize { QString text; int speakerId; float speed; QVariantMap settings; };
    struct EventCloneVoice { QString text; QString referencePath; QVariantMap settings; };
    struct EventCancelProcessing {};
    struct EventWorkerFinished { QVector<float> samples; int sampleRate; };
    struct EventWorkerError { QString error; };

    using EngineEvent = std::variant<EventLoadVoice, EventUnload, EventWorkerLoaded, EventSynthesize, EventCloneVoice, EventCancelProcessing, EventWorkerFinished, EventWorkerError>;

    explicit TtsEngineInstance(QObject *parent = nullptr);
    ~TtsEngineInstance() override;

    State state() const;
    bool isModelLoaded() const { State s = state(); return s == Ready || s == Processing; }
    bool isProcessing() const { return state() == Processing; }
    int sampleRate() const { return m_sampleRate; }
    QVariantList currentSchema() const { return m_currentSchema; }
    QVariantMap familyConfig() const { return m_familyConfig; }
    void setFamilyConfig(const QVariantMap &config);
    qint64 estimatedRamBytes() const { return m_estimatedRamBytes; }
    qint64 estimatedVramBytes() const { return m_estimatedVramBytes; }
    QString estimatedRamUsage() const;
    QString estimatedVramUsage() const;
    double cpuUsage() const { return m_cpuUsage; }
    bool isCloneAction() const { return m_isCloneAction; }
    QString lastGenerationMode() const { return m_lastGenerationMode; }
    int generationProgress() const { return m_generationProgress; }
    bool generationProgressEstimated() const { return m_generationProgressEstimated; }
    QString generationProgressLabel() const { return m_generationProgressLabel; }


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


    QByteArray lastPcm() const { return m_lastPcm; }
    QVector<float> lastSamples() const { return m_lastSamples; }
    QVariantList lastSamplePreview() const { return m_lastSamplePreview; }
    int lastSampleCount() const { return m_lastSamples.size(); }

signals:
    void modelLoadedChanged();
    void processingChanged();
    void sampleRateChanged();
    void outputChanged();
    void synthesisFinished(const QByteArray &pcm16, int sampleRate);
    void errorOccurred(const QString &error);
    void schemaChanged();
    void familyConfigChanged();
    void memoryUsageChanged();
    void cpuUsageChanged();
    void stateChanged();
    void lastGenerationModeChanged();
    void generationProgressChanged();


private slots:
    void onWorkerModelLoaded(bool success, const QString &error, const QVariantList &schema);
    void onWorkerFinished(const QVector<float> &samples, int sampleRate);
    void onWorkerError(const QString &error);
    void onWorkerProgress(int current, int total, const QString &stage, int chunkIndex, int chunkCount);
    void updateCpuUsage();

private:
    QVariantList buildSchemaForRuntime(const QString &runtimePath) const;
    void updateMemoryUsageEstimates();
    static QString formatBytes(qint64 bytes);
    
    void dispatch(const EngineEvent &event);
    void applyState(const EngineState &newState);
    QString voiceConfigSignature(const QVariantMap &config) const;
    void rememberLoadingVoiceConfig(const QVariantMap &config);
    void rememberLoadedVoiceConfig();
    void clearVoiceConfigTracking();
    void resetGenerationProgress();
    void setGenerationProgress(int progress, bool estimated, const QString &label);

    TtsWorker *m_worker = nullptr;
    QThread *m_thread = nullptr;
    EngineState m_state = StateUnloaded{};
    int m_sampleRate = 22050;
    QByteArray m_lastPcm;
    QVector<float> m_lastSamples;
    QVariantList m_lastSamplePreview;
    QVariantList m_currentSchema;
    QVariantMap m_familyConfig;
    QVariantMap m_lastVoiceConfig;
    QString m_lastRuntimePath;
    QString m_lastModelPath;
    QString m_lastCodecPath;
    QString m_loadingVoiceSignature;
    QString m_loadedVoiceSignature;
    QHash<QString, QVariantList> m_schemaCache;
    bool m_memoryBaselineArmed = false;
    double m_memoryBaselineRamGb = 0.0;
    double m_memoryBaselineVramGb = 0.0;
    qint64 m_estimatedRamBytes = 0;
    qint64 m_estimatedVramBytes = 0;
    double m_cpuUsage = 0;
    QTimer *m_cpuTimer = nullptr;
    bool m_isCloneAction = false;
    QString m_lastGenerationMode = QStringLiteral("tts");
    int m_generationProgress = 0;
    bool m_generationProgressEstimated = true;
    QString m_generationProgressLabel = QStringLiteral("Generating audio");
};

} // namespace LAStudio
