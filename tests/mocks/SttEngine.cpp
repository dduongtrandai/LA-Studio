#include "stt/SttEngine.h"
#include <QTimer>

namespace LAStudio {

static SttEngine::State s_mockState = SttEngine::Unloaded;
static int s_mockProgress = 0;
static QString s_mockTranscript;
static QString s_activeSignature;

SttEngine::SttEngine(QObject *parent)
    : QObject(parent)
{
    s_mockState = SttEngine::Unloaded;
    s_mockProgress = 0;
    s_mockTranscript.clear();
    s_activeSignature.clear();
}

SttEngine::~SttEngine()
{
}

SttEngine::State SttEngine::state() const
{
    return s_mockState;
}

bool SttEngine::isModelLoaded() const
{
    return s_mockState == Ready || s_mockState == Processing;
}

bool SttEngine::isProcessing() const
{
    return s_mockState == Processing;
}

int SttEngine::progress() const
{
    return s_mockProgress;
}

QString SttEngine::transcript() const
{
    return s_mockTranscript;
}

void SttEngine::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath)
{
    Q_UNUSED(modelPath);
    Q_UNUSED(useGpu);
    Q_UNUSED(runtimePath);

    s_mockState = Loading;
    emit stateChanged();
    emit modelLoadedChanged();

    // Simulate model loading asynchronously
    QTimer::singleShot(50, this, [this]() {
        s_mockState = Ready;
        emit stateChanged();
        emit modelLoadedChanged();
    });
}

void SttEngine::unloadModel()
{
    s_mockState = Unloaded;
    emit stateChanged();
    emit modelLoadedChanged();
}

void SttEngine::unloadModelSync()
{
    unloadModel();
}

void SttEngine::transcribeSamples(const QVector<float> &samples, const QString &language, int threads, bool translate, const QVariantMap &settings)
{
    Q_UNUSED(samples);
    Q_UNUSED(language);
    Q_UNUSED(threads);
    Q_UNUSED(translate);
    Q_UNUSED(settings);

    s_mockState = Processing;
    emit stateChanged();
    emit processingChanged();

    // Simulate transcription asynchronously
    QTimer::singleShot(50, this, [this]() {
        s_mockProgress = 100;
        s_mockTranscript = QStringLiteral("Mock transcribed text");
        emit progressChanged();
        emit transcriptChanged();

        s_mockState = Ready;
        emit stateChanged();
        emit processingChanged();

        emit transcriptionFinished(s_mockTranscript, QVariantList());
    });
}

void SttEngine::cancelProcessing()
{
}

SttEngineInstance *SttEngine::loadInstance(const SessionConfiguration &config, bool useGpu)
{
    Q_UNUSED(config);
    Q_UNUSED(useGpu);
    return nullptr;
}

bool SttEngine::activateInstance(const QString &signature)
{
    s_activeSignature = signature;
    emit activeSignatureChanged();
    return true;
}

void SttEngine::unloadInstance(const QString &signature)
{
    Q_UNUSED(signature);
}

SttEngineInstance *SttEngine::instance(const QString &signature) const
{
    Q_UNUSED(signature);
    return nullptr;
}

QList<SttEngineInstance *> SttEngine::loadedInstances() const
{
    return {};
}

QStringList SttEngine::loadedSignatures() const
{
    return {};
}

} // namespace LAStudio
