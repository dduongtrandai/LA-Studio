#include "tts/TtsEngine.h"

namespace LAStudio {

static TtsEngine::State s_mockState = TtsEngine::Unloaded;
static QVector<float> s_mockLastSamples = {0.1f, -0.1f, 0.2f, -0.2f};
static int s_mockSampleRate = 16000;
static QByteArray s_mockLastPcm;
static QVariantList s_mockLastSamplePreview;
static int s_mockLastSampleCount = 4;
static QVariantList s_mockCurrentSchema;
static QVariantMap s_mockFamilyConfig;
static double s_mockCpuUsage = 0.0;
static bool s_mockIsCloneAction = false;
static QString s_mockLastGenerationMode = QStringLiteral("tts");
static int s_mockGenerationProgress = 0;
static bool s_mockGenerationProgressEstimated = true;
static QString s_mockGenerationProgressLabel = QStringLiteral("Generating audio");
static QString s_mockActiveSignature;

TtsEngine::TtsEngine(QObject *parent)
    : QObject(parent)
{
    s_mockState = TtsEngine::Unloaded;
    s_mockLastSamples = {0.1f, -0.1f, 0.2f, -0.2f};
    s_mockSampleRate = 16000;
    s_mockLastPcm.clear();
    s_mockLastSamplePreview.clear();
    s_mockLastSampleCount = 4;
    s_mockCurrentSchema.clear();
    s_mockFamilyConfig.clear();
    s_mockCpuUsage = 0.0;
    s_mockIsCloneAction = false;
    s_mockLastGenerationMode = QStringLiteral("tts");
    s_mockGenerationProgress = 0;
    s_mockGenerationProgressEstimated = true;
    s_mockGenerationProgressLabel = QStringLiteral("Generating audio");
    s_mockActiveSignature.clear();
}

TtsEngine::~TtsEngine()
{
}

TtsEngine::State TtsEngine::state() const
{
    return s_mockState;
}

bool TtsEngine::isModelLoaded() const
{
    return s_mockState == Ready || s_mockState == Processing;
}

bool TtsEngine::isProcessing() const
{
    return s_mockState == Processing;
}

int TtsEngine::sampleRate() const
{
    return s_mockSampleRate;
}

QVariantList TtsEngine::currentSchema() const
{
    return s_mockCurrentSchema;
}

QVariantMap TtsEngine::familyConfig() const
{
    return s_mockFamilyConfig;
}

void TtsEngine::setFamilyConfig(const QVariantMap &config)
{
    s_mockFamilyConfig = config;
    emit familyConfigChanged();
    emit schemaChanged();
}

qint64 TtsEngine::estimatedRamBytes() const
{
    return 0;
}

qint64 TtsEngine::estimatedVramBytes() const
{
    return 0;
}

QString TtsEngine::estimatedRamUsage() const
{
    return QStringLiteral("0 MB");
}

QString TtsEngine::estimatedVramUsage() const
{
    return QStringLiteral("0 MB");
}

double TtsEngine::cpuUsage() const
{
    return s_mockCpuUsage;
}

bool TtsEngine::isCloneAction() const
{
    return s_mockIsCloneAction;
}

QString TtsEngine::lastGenerationMode() const
{
    return s_mockLastGenerationMode;
}

int TtsEngine::generationProgress() const
{
    return s_mockGenerationProgress;
}

bool TtsEngine::generationProgressEstimated() const
{
    return s_mockGenerationProgressEstimated;
}

QString TtsEngine::generationProgressLabel() const
{
    return s_mockGenerationProgressLabel;
}

QVariantList TtsEngine::schemaForCapability(const QString &capability) const
{
    Q_UNUSED(capability);
    return {};
}

QVariantMap TtsEngine::studioConfigForCapability(const QString &capability) const
{
    Q_UNUSED(capability);
    return {};
}

void TtsEngine::loadVoice(const QVariantMap &config)
{
    Q_UNUSED(config);
    s_mockState = Ready;
    emit stateChanged();
    emit modelLoadedChanged();
}

void TtsEngine::loadModel(const QString &modelPath, const QString &codecPath, const QString &runtimePath)
{
    Q_UNUSED(modelPath);
    Q_UNUSED(codecPath);
    Q_UNUSED(runtimePath);
    s_mockState = Ready;
    emit stateChanged();
    emit modelLoadedChanged();
}

void TtsEngine::loadModel(const QVariantMap &filesByRole, const QString &runtimePath)
{
    Q_UNUSED(filesByRole);
    Q_UNUSED(runtimePath);
    s_mockState = Ready;
    emit stateChanged();
    emit modelLoadedChanged();
}

void TtsEngine::setRuntimePath(const QString &runtimePath)
{
    Q_UNUSED(runtimePath);
}

void TtsEngine::unloadVoice()
{
    s_mockState = Unloaded;
    emit stateChanged();
    emit modelLoadedChanged();
}

void TtsEngine::unloadVoiceSync()
{
    unloadVoice();
}

void TtsEngine::clearLastSamples()
{
    s_mockLastSamples.clear();
    s_mockLastPcm.clear();
    s_mockLastSamplePreview.clear();
    s_mockLastSampleCount = 0;
}

void TtsEngine::synthesize(const QString &text, int speakerId, float speed, const QVariantMap &settings)
{
    Q_UNUSED(text);
    Q_UNUSED(speakerId);
    Q_UNUSED(speed);
    Q_UNUSED(settings);
}

void TtsEngine::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings)
{
    Q_UNUSED(text);
    Q_UNUSED(referencePath);
    Q_UNUSED(settings);
}

void TtsEngine::designVoice(const QString &text, const QVariantMap &settings)
{
    Q_UNUSED(text);
    Q_UNUSED(settings);
}

void TtsEngine::cancelProcessing()
{
}

QByteArray TtsEngine::lastPcm() const
{
    return s_mockLastPcm;
}

QVector<float> TtsEngine::lastSamples() const
{
    return s_mockLastSamples;
}

QVariantList TtsEngine::lastSamplePreview() const
{
    return s_mockLastSamplePreview;
}

int TtsEngine::lastSampleCount() const
{
    return s_mockLastSampleCount;
}

TtsEngineInstance *TtsEngine::loadInstance(const SessionConfiguration &config)
{
    Q_UNUSED(config);
    return nullptr;
}

bool TtsEngine::activateInstance(const QString &signature)
{
    s_mockActiveSignature = signature;
    emit activeSignatureChanged();
    return true;
}

void TtsEngine::unloadInstance(const QString &signature)
{
    Q_UNUSED(signature);
}

TtsEngineInstance *TtsEngine::instance(const QString &signature) const
{
    Q_UNUSED(signature);
    return nullptr;
}

QList<TtsEngineInstance *> TtsEngine::loadedInstances() const
{
    return {};
}

QStringList TtsEngine::loadedSignatures() const
{
    return {};
}

} // namespace LAStudio
