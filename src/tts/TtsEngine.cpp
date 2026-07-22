#include "TtsEngine.h"
#include "core/Logger.h"
#include <runtimes/CrispKokoroInterface.h>
#include <runtimes/CrispQwen3TtsInterface.h>
#include <runtimes/CrispVibeVoiceInterface.h>
#include <runtimes/CrispVoxCpm2Interface.h>

#include <QFileInfo>

namespace LAStudio {

namespace {
void unloadCrispTtsRuntimes()
{
    CrispKokoroInterface::instance().unload();
    CrispQwen3TtsInterface::instance().unload();
    CrispVibeVoiceInterface::instance().unload();
    CrispVoxCpm2Interface::instance().unload();
}
}

TtsEngine::TtsEngine(QObject *parent)
    : QObject(parent)
{
}

TtsEngine::~TtsEngine()
{
    qDeleteAll(m_instances.values());
    m_instances.clear();
    unloadCrispTtsRuntimes();
}

TtsEngineInstance *TtsEngine::activeInstance() const
{
    return m_instances.value(m_activeSignature);
}

TtsEngine::State TtsEngine::state() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? static_cast<TtsEngine::State>(inst->state()) : TtsEngine::Unloaded;
}

bool TtsEngine::isModelLoaded() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst && inst->isModelLoaded();
}

bool TtsEngine::isProcessing() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst && inst->isProcessing();
}

int TtsEngine::sampleRate() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->sampleRate() : 22050;
}

QVariantList TtsEngine::currentSchema() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->currentSchema() : QVariantList();
}

QVariantMap TtsEngine::familyConfig() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->familyConfig() : m_familyConfig;
}

void TtsEngine::setFamilyConfig(const QVariantMap &config)
{
    m_familyConfig = config;
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->setFamilyConfig(config);
    }
    emit familyConfigChanged();
    emit schemaChanged();
}

qint64 TtsEngine::estimatedRamBytes() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->estimatedRamBytes() : 0;
}

qint64 TtsEngine::estimatedVramBytes() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->estimatedVramBytes() : 0;
}

QString TtsEngine::estimatedRamUsage() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->estimatedRamUsage() : QStringLiteral("0 MB");
}

QString TtsEngine::estimatedVramUsage() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->estimatedVramUsage() : QStringLiteral("0 MB");
}

double TtsEngine::cpuUsage() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->cpuUsage() : 0.0;
}

bool TtsEngine::isCloneAction() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst && inst->isCloneAction();
}

QString TtsEngine::lastGenerationMode() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->lastGenerationMode() : QStringLiteral("tts");
}

int TtsEngine::generationProgress() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->generationProgress() : 0;
}

bool TtsEngine::generationProgressEstimated() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->generationProgressEstimated() : true;
}

QString TtsEngine::generationProgressLabel() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->generationProgressLabel() : QStringLiteral("Generating audio");
}

QVariantList TtsEngine::schemaForCapability(const QString &capability) const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->schemaForCapability(capability) : QVariantList();
}

QVariantMap TtsEngine::studioConfigForCapability(const QString &capability) const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->studioConfigForCapability(capability) : QVariantMap();
}

QByteArray TtsEngine::lastPcm() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->lastPcm() : QByteArray();
}

QVector<float> TtsEngine::lastSamples() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->lastSamples() : QVector<float>();
}

QVariantList TtsEngine::lastSamplePreview() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->lastSamplePreview() : QVariantList();
}

int TtsEngine::lastSampleCount() const
{
    TtsEngineInstance *inst = activeInstance();
    return inst ? inst->lastSampleCount() : 0;
}

void TtsEngine::loadVoice(const QVariantMap &config)
{
    const QString signature = QFileInfo(config.value(QStringLiteral("model")).toString()).absoluteFilePath()
        + QLatin1Char('|') + QFileInfo(config.value(QStringLiteral("codec")).toString()).absoluteFilePath()
        + QLatin1Char('|') + config.value(QStringLiteral("runtimePath")).toString();
    TtsEngineInstance *inst = ensureInstance(signature);
    activateInstance(signature);
    inst->setFamilyConfig(m_familyConfig);
    if (!m_runtimePath.isEmpty()) {
        inst->setRuntimePath(m_runtimePath);
    }
    inst->loadVoice(config);
}

void TtsEngine::loadModel(const QString &modelPath, const QString &codecPath, const QString &runtimePath)
{
    QVariantMap config;
    config.insert(QStringLiteral("model"), modelPath);
    if (!codecPath.isEmpty()) {
        config.insert(QStringLiteral("codec"), codecPath);
    }
    if (!runtimePath.isEmpty()) {
        config.insert(QStringLiteral("runtimePath"), runtimePath);
    }
    loadVoice(config);
}

void TtsEngine::loadModel(const QVariantMap &filesByRole, const QString &runtimePath)
{
    QVariantMap config = filesByRole;
    if (!config.contains(QStringLiteral("codec")) && config.contains(QStringLiteral("tokenizer"))) {
        config.insert(QStringLiteral("codec"), config.value(QStringLiteral("tokenizer")));
    }
    if (!runtimePath.isEmpty()) {
        config.insert(QStringLiteral("runtimePath"), runtimePath);
    }
    loadVoice(config);
}

void TtsEngine::setRuntimePath(const QString &runtimePath)
{
    m_runtimePath = runtimePath;
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->setRuntimePath(runtimePath);
    }
}

void TtsEngine::unloadVoice()
{
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->unloadVoice();
    }
}

void TtsEngine::unloadVoiceSync()
{
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->unloadVoiceSync();
    }
}

void TtsEngine::clearLastSamples()
{
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->clearLastSamples();
    }
}

void TtsEngine::synthesize(const QString &text, int speakerId, float speed, const QVariantMap &settings)
{
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->synthesize(text, speakerId, speed, settings);
        return;
    }
    emit errorOccurred(QStringLiteral("No TTS model is active."));
}

void TtsEngine::cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings)
{
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->cloneVoice(text, referencePath, settings);
        return;
    }
    emit errorOccurred(QStringLiteral("No TTS model is active."));
}

void TtsEngine::designVoice(const QString &text, const QVariantMap &settings)
{
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->designVoice(text, settings);
        return;
    }
    emit errorOccurred(QStringLiteral("No TTS model is active."));
}

void TtsEngine::cancelProcessing()
{
    if (TtsEngineInstance *inst = activeInstance()) {
        inst->cancelProcessing();
    }
}

TtsEngineInstance *TtsEngine::loadInstance(const SessionConfiguration &config)
{
#ifdef Q_OS_WIN
    const QStringList signatures = m_instances.signatures();
    for (const QString &signature : signatures) {
        if (signature == config.signature) {
            continue;
        }

        TtsEngineInstance *oldInstance = m_instances.take(signature);
        if (!oldInstance) {
            continue;
        }

        Logger::info(QStringLiteral("TtsEngine"),
                     QStringLiteral("Unloading previous TTS runtime instance before loading: %1").arg(config.signature));
        oldInstance->unloadVoiceSync();
        oldInstance->deleteLater();
    }

    if (!m_instances.contains(m_activeSignature)) {
        m_activeSignature.clear();
        emit activeSignatureChanged();
        emit loadedInstancesChanged();
    }
#endif

    TtsEngineInstance *inst = ensureInstance(config.signature);
    inst->setFamilyConfig(config.familyConfig);
    inst->setRuntimePath(config.runtimePath);
    inst->loadModel(config.resolvedPathsByRole, config.runtimePath);
    activateInstance(config.signature);
    return inst;
}

bool TtsEngine::activateInstance(const QString &signature)
{
    if (signature.isEmpty() || !m_instances.contains(signature)) {
        return false;
    }
    if (m_activeSignature == signature) {
        return true;
    }
    m_activeSignature = signature;
    emit activeSignatureChanged();
    emitActiveForwardSignals();
    return true;
}

void TtsEngine::unloadInstance(const QString &signature)
{
    TtsEngineInstance *inst = m_instances.take(signature);
    if (!inst) {
        return;
    }

    const bool wasActive = signature == m_activeSignature;
    inst->unloadVoiceSync();
    inst->deleteLater();

    if (m_instances.size() == 0) {
        // The backend session is gone, but the CrispASR interface singleton
        // still owns the DLL and its preloaded GGML dependencies.
        unloadCrispTtsRuntimes();
    }

    if (wasActive) {
        m_activeSignature = m_instances.size() == 0 ? QString() : m_instances.signatures().last();
        emit activeSignatureChanged();
        emitActiveForwardSignals();
    }
    emit loadedInstancesChanged();
}

TtsEngineInstance *TtsEngine::instance(const QString &signature) const
{
    return m_instances.value(signature);
}

QList<TtsEngineInstance *> TtsEngine::loadedInstances() const
{
    return m_instances.values();
}

QStringList TtsEngine::loadedSignatures() const
{
    return m_instances.signatures();
}

TtsEngineInstance *TtsEngine::ensureInstance(const QString &signature)
{
    const QString key = signature.isEmpty() ? QStringLiteral("default") : signature;
    if (TtsEngineInstance *existing = m_instances.value(key)) {
        return existing;
    }

    auto *inst = new TtsEngineInstance(this);
    m_instances.insert(key, inst);
    connectInstance(inst);
    emit loadedInstancesChanged();
    return inst;
}

void TtsEngine::connectInstance(TtsEngineInstance *inst)
{
    connect(inst, &TtsEngineInstance::errorOccurred, this, &TtsEngine::errorOccurred);
    auto forwardIfActive = [this, inst](auto signal, auto emitSignal) {
        connect(inst, signal, this, [this, inst, emitSignal]() {
            if (inst == activeInstance()) {
                (this->*emitSignal)();
            }
        });
    };
    forwardIfActive(&TtsEngineInstance::modelLoadedChanged, &TtsEngine::modelLoadedChanged);
    forwardIfActive(&TtsEngineInstance::processingChanged, &TtsEngine::processingChanged);
    forwardIfActive(&TtsEngineInstance::sampleRateChanged, &TtsEngine::sampleRateChanged);
    forwardIfActive(&TtsEngineInstance::outputChanged, &TtsEngine::outputChanged);
    forwardIfActive(&TtsEngineInstance::schemaChanged, &TtsEngine::schemaChanged);
    forwardIfActive(&TtsEngineInstance::familyConfigChanged, &TtsEngine::familyConfigChanged);
    forwardIfActive(&TtsEngineInstance::memoryUsageChanged, &TtsEngine::memoryUsageChanged);
    forwardIfActive(&TtsEngineInstance::cpuUsageChanged, &TtsEngine::cpuUsageChanged);
    forwardIfActive(&TtsEngineInstance::stateChanged, &TtsEngine::stateChanged);
    forwardIfActive(&TtsEngineInstance::lastGenerationModeChanged, &TtsEngine::lastGenerationModeChanged);
    forwardIfActive(&TtsEngineInstance::generationProgressChanged, &TtsEngine::generationProgressChanged);
    connect(inst, &TtsEngineInstance::synthesisFinished, this, [this, inst](const QByteArray &pcm, int sr) {
        if (inst == activeInstance()) {
            emit synthesisFinished(pcm, sr);
        }
    });
}

void TtsEngine::emitActiveForwardSignals()
{
    emit stateChanged();
    emit modelLoadedChanged();
    emit processingChanged();
    emit sampleRateChanged();
    emit outputChanged();
    emit schemaChanged();
    emit familyConfigChanged();
    emit memoryUsageChanged();
    emit cpuUsageChanged();
    emit lastGenerationModeChanged();
    emit generationProgressChanged();
}

} // namespace LAStudio
