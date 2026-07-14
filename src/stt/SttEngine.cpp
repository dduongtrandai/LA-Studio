#include "SttEngine.h"
#include <QFileInfo>

namespace LAStudio {

SttEngine::SttEngine(QObject *parent)
    : QObject(parent)
{
}

SttEngine::~SttEngine()
{
    qDeleteAll(m_instances);
    m_instances.clear();
}

SttEngineInstance *SttEngine::activeInstance() const
{
    return m_instances.value(m_activeSignature, nullptr);
}

SttEngine::State SttEngine::state() const
{
    SttEngineInstance *inst = activeInstance();
    return inst ? static_cast<SttEngine::State>(inst->state()) : SttEngine::Unloaded;
}

bool SttEngine::isModelLoaded() const
{
    SttEngineInstance *inst = activeInstance();
    return inst && inst->isModelLoaded();
}

bool SttEngine::isProcessing() const
{
    SttEngineInstance *inst = activeInstance();
    return inst && inst->isProcessing();
}

int SttEngine::progress() const
{
    SttEngineInstance *inst = activeInstance();
    return inst ? inst->progress() : 0;
}

QString SttEngine::transcript() const
{
    SttEngineInstance *inst = activeInstance();
    return inst ? inst->transcript() : QString();
}

void SttEngine::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath)
{
    const QString signature = QFileInfo(modelPath).absoluteFilePath() + QLatin1Char('|') + runtimePath + QLatin1Char('|') + (useGpu ? QStringLiteral("gpu") : QStringLiteral("cpu"));
    SttEngineInstance *inst = ensureInstance(signature);
    activateInstance(signature);
    inst->loadModel(modelPath, useGpu, runtimePath);
}

void SttEngine::unloadModel()
{
    if (SttEngineInstance *inst = activeInstance()) {
        inst->unloadModel();
    }
}

void SttEngine::unloadModelSync()
{
    if (SttEngineInstance *inst = activeInstance()) {
        inst->unloadModelSync();
    }
}

void SttEngine::transcribeSamples(const QVector<float> &samples, const QString &language, int threads, bool translate, const QVariantMap &settings)
{
    if (SttEngineInstance *inst = activeInstance()) {
        inst->transcribeSamples(samples, language, threads, translate, settings);
        return;
    }
    emit errorOccurred(QStringLiteral("No STT model is active."));
}

void SttEngine::cancelProcessing()
{
    if (SttEngineInstance *inst = activeInstance()) {
        inst->cancelProcessing();
    }
}

SttEngineInstance *SttEngine::loadInstance(const SessionConfiguration &config, bool useGpu)
{
    SttEngineInstance *inst = ensureInstance(config.signature);
    const QString modelPath = config.resolvedPathsByRole.value(QStringLiteral("model")).toString();
    inst->loadModel(modelPath, useGpu, config.runtimePath);
    activateInstance(config.signature);
    return inst;
}

bool SttEngine::activateInstance(const QString &signature)
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

void SttEngine::unloadInstance(const QString &signature)
{
    SttEngineInstance *inst = m_instances.take(signature);
    if (!inst) {
        return;
    }

    const bool wasActive = signature == m_activeSignature;
    inst->unloadModelSync();
    inst->deleteLater();

    if (wasActive) {
        m_activeSignature = m_instances.isEmpty() ? QString() : m_instances.keys().last();
        emit activeSignatureChanged();
        emitActiveForwardSignals();
    }
    emit loadedInstancesChanged();
}

SttEngineInstance *SttEngine::instance(const QString &signature) const
{
    return m_instances.value(signature, nullptr);
}

QList<SttEngineInstance *> SttEngine::loadedInstances() const
{
    return m_instances.values();
}

QStringList SttEngine::loadedSignatures() const
{
    return m_instances.keys();
}

SttEngineInstance *SttEngine::ensureInstance(const QString &signature)
{
    const QString key = signature.isEmpty() ? QStringLiteral("default") : signature;
    if (SttEngineInstance *existing = m_instances.value(key, nullptr)) {
        return existing;
    }

    auto *inst = new SttEngineInstance(this);
    m_instances.insert(key, inst);
    connectInstance(inst);
    emit loadedInstancesChanged();
    return inst;
}

void SttEngine::connectInstance(SttEngineInstance *inst)
{
    connect(inst, &SttEngineInstance::errorOccurred, this, &SttEngine::errorOccurred);
    connect(inst, &SttEngineInstance::transcriptionFinished, this, [this, inst](const QString &text, const QVariantList &segments) {
        if (inst == activeInstance()) {
            emit transcriptionFinished(text, segments);
        }
    });

    auto forwardIfActive = [this, inst](auto signal) {
        connect(inst, signal, this, [this, inst]() {
            if (inst == activeInstance()) {
                emitActiveForwardSignals();
            }
        });
    };
    forwardIfActive(&SttEngineInstance::modelLoadedChanged);
    forwardIfActive(&SttEngineInstance::processingChanged);
    forwardIfActive(&SttEngineInstance::progressChanged);
    forwardIfActive(&SttEngineInstance::transcriptChanged);
    forwardIfActive(&SttEngineInstance::stateChanged);
}

void SttEngine::emitActiveForwardSignals()
{
    emit stateChanged();
    emit modelLoadedChanged();
    emit processingChanged();
    emit progressChanged();
    emit transcriptChanged();
}

} // namespace LAStudio
