#include "TranslationEngine.h"

namespace LAStudio {

TranslationEngine::TranslationEngine(std::shared_ptr<TranslationBackendFactory> factory,
                                     QObject *parent)
    : QObject(parent), m_factory(factory ? std::move(factory) : std::make_shared<TranslationBackendFactory>())
{
}

TranslationEngine::~TranslationEngine()
{
    for (TranslationEngineInstance *instance : m_instances.values()) {
        instance->unloadModelSync();
        delete instance;
    }
    m_instances.clear();
}

TranslationEngineInstance *TranslationEngine::activeInstance() const
{
    return m_instances.value(m_activeSignature);
}

TranslationEngine::State TranslationEngine::state() const
{
    const auto *instance = activeInstance();
    return instance ? static_cast<State>(instance->state()) : Unloaded;
}

bool TranslationEngine::isModelLoaded() const
{
    const auto *instance = activeInstance();
    return instance && instance->isModelLoaded();
}

bool TranslationEngine::isProcessing() const
{
    const auto *instance = activeInstance();
    return instance && instance->isProcessing();
}

int TranslationEngine::progress() const
{
    const auto *instance = activeInstance();
    return instance ? instance->progress() : 0;
}

TranslationEngineInstance *TranslationEngine::ensureInstance(const SessionConfiguration &configuration)
{
    const QString signature = configuration.signature;
    if (signature.isEmpty()) return nullptr;
    if (auto *existing = m_instances.value(signature)) return existing;
    auto *created = new TranslationEngineInstance(configuration, m_factory, this);
    m_instances.insert(signature, created);
    connectInstance(created);
    emit loadedInstancesChanged();
    return created;
}

TranslationEngineInstance *TranslationEngine::loadInstance(const SessionConfiguration &configuration,
                                                           bool activate)
{
    auto *instance = ensureInstance(configuration);
    if (!instance) return nullptr;
    if (activate && m_activeSignature != configuration.signature) {
        m_activeSignature = configuration.signature;
        emit activeSignatureChanged();
        emitActiveForwardSignals();
    }
    if (!instance->isModelLoaded()) instance->loadModel();
    return instance;
}

bool TranslationEngine::activateInstance(const QString &signature)
{
    if (!m_instances.contains(signature)) return false;
    if (m_activeSignature == signature) return true;
    m_activeSignature = signature;
    emit activeSignatureChanged();
    emitActiveForwardSignals();
    return true;
}

void TranslationEngine::unloadInstance(const QString &signature)
{
    auto *instance = m_instances.take(signature);
    if (!instance) return;
    const bool wasActive = m_activeSignature == signature;
    instance->unloadModelSync();
    instance->deleteLater();
    emit loadedInstancesChanged();
    if (wasActive) {
        const QStringList remaining = m_instances.signatures();
        m_activeSignature = remaining.isEmpty() ? QString() : remaining.constLast();
        emit activeSignatureChanged();
        emitActiveForwardSignals();
    }
}

TranslationEngineInstance *TranslationEngine::instance(const QString &signature) const
{
    return m_instances.value(signature);
}

QList<TranslationEngineInstance *> TranslationEngine::loadedInstances() const
{
    return m_instances.values();
}

QList<SessionConfiguration> TranslationEngine::loadedConfigurations() const
{
    QList<SessionConfiguration> result;
    for (auto *instance : m_instances.values()) {
        if (instance) result.append(instance->configuration());
    }
    return result;
}

QStringList TranslationEngine::loadedSignatures() const
{
    return m_instances.signatures();
}

void TranslationEngine::translate(const TranslationInferenceRequest &request)
{
    if (auto *instance = activeInstance()) instance->translate(request);
    else emit errorOccurred(QStringLiteral("Select and load a Translation model first."));
}

void TranslationEngine::cancelProcessing()
{
    if (auto *instance = activeInstance()) instance->cancelProcessing();
}

void TranslationEngine::connectInstance(TranslationEngineInstance *instance)
{
    connect(instance, &TranslationEngineInstance::errorOccurred,
            this, &TranslationEngine::errorOccurred);
    connect(instance, &TranslationEngineInstance::translationFinished,
            this, [this, instance](const QVariantList &patches) {
        if (activeInstance() == instance) emit translationFinished(patches);
    });
    connect(instance, &TranslationEngineInstance::stateChanged,
            this, [this, instance]() {
        if (activeInstance() != instance) return;
        emit modelLoadedChanged();
        emit processingChanged();
        emit progressChanged();
        emit stateChanged();
    });
    connect(instance, &TranslationEngineInstance::progressChanged,
            this, [this, instance]() { if (activeInstance() == instance) emit progressChanged(); });
}

void TranslationEngine::emitActiveForwardSignals()
{
    emit modelLoadedChanged();
    emit processingChanged();
    emit progressChanged();
    emit stateChanged();
}

} // namespace LAStudio
