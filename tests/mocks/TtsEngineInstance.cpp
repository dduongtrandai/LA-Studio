#include "tts/TtsEngineInstance.h"

namespace LAStudio {

TtsEngineInstance::TtsEngineInstance(QObject *parent)
    : QObject(parent)
{
}

TtsEngineInstance::~TtsEngineInstance() = default;

TtsEngineInstance::State TtsEngineInstance::state() const
{
    return Ready;
}

QString TtsEngineInstance::estimatedRamUsage() const
{
    return QStringLiteral("0 MB");
}

QString TtsEngineInstance::estimatedVramUsage() const
{
    return QStringLiteral("0 MB");
}

void TtsEngineInstance::setFamilyConfig(const QVariantMap &config)
{
    Q_UNUSED(config);
}

QVariantList TtsEngineInstance::schemaForCapability(const QString &capability) const
{
    Q_UNUSED(capability);
    return {};
}

QVariantMap TtsEngineInstance::studioConfigForCapability(const QString &capability) const
{
    Q_UNUSED(capability);
    return {};
}

void TtsEngineInstance::loadVoice(const QVariantMap &config)
{
    Q_UNUSED(config);
}

void TtsEngineInstance::loadModel(const QString &modelPath, const QString &codecPath, const QString &runtimePath)
{
    Q_UNUSED(modelPath);
    Q_UNUSED(codecPath);
    Q_UNUSED(runtimePath);
}

void TtsEngineInstance::loadModel(const QVariantMap &filesByRole, const QString &runtimePath)
{
    Q_UNUSED(filesByRole);
    Q_UNUSED(runtimePath);
}

void TtsEngineInstance::setRuntimePath(const QString &runtimePath)
{
    Q_UNUSED(runtimePath);
}

void TtsEngineInstance::unloadVoice()
{
}

void TtsEngineInstance::unloadVoiceSync()
{
}

void TtsEngineInstance::clearLastSamples()
{
}

void TtsEngineInstance::synthesize(const QString &text, int speakerId, float speed, const QVariantMap &settings)
{
    Q_UNUSED(text);
    Q_UNUSED(speakerId);
    Q_UNUSED(speed);
    Q_UNUSED(settings);
    emit synthesisFinished(QByteArray(), 24000);
}

void TtsEngineInstance::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings)
{
    Q_UNUSED(referencePath);
    synthesize(text, 0, 1.0f, settings);
}

void TtsEngineInstance::designVoice(const QString &text, const QVariantMap &settings)
{
    synthesize(text, 0, 1.0f, settings);
}

void TtsEngineInstance::cancelProcessing()
{
}

void TtsEngineInstance::onWorkerModelLoaded(bool success, const QString &error, const QVariantList &schema)
{
    Q_UNUSED(success);
    Q_UNUSED(error);
    Q_UNUSED(schema);
}

void TtsEngineInstance::onWorkerFinished(const QVector<float> &samples, int sampleRate)
{
    Q_UNUSED(samples);
    Q_UNUSED(sampleRate);
}

void TtsEngineInstance::onWorkerError(const QString &error)
{
    Q_UNUSED(error);
}

void TtsEngineInstance::onWorkerProgress(int current, int total, const QString &stage, int chunkIndex, int chunkCount)
{
    Q_UNUSED(current);
    Q_UNUSED(total);
    Q_UNUSED(stage);
    Q_UNUSED(chunkIndex);
    Q_UNUSED(chunkCount);
}

void TtsEngineInstance::updateCpuUsage()
{
}

} // namespace LAStudio
