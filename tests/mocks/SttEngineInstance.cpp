#include "stt/SttEngineInstance.h"

namespace LAStudio {

SttEngineInstance::SttEngineInstance(QObject *parent)
    : QObject(parent)
{
}

SttEngineInstance::~SttEngineInstance() = default;

SttEngineInstance::State SttEngineInstance::state() const
{
    return Ready;
}

void SttEngineInstance::loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath)
{
    Q_UNUSED(modelPath);
    Q_UNUSED(useGpu);
    Q_UNUSED(runtimePath);
}

void SttEngineInstance::unloadModel()
{
}

void SttEngineInstance::unloadModelSync()
{
}

void SttEngineInstance::transcribeSamples(const QVector<float> &samples,
                                          const QString &language,
                                          int threads,
                                          bool translate,
                                          const QVariantMap &settings)
{
    Q_UNUSED(samples);
    Q_UNUSED(language);
    Q_UNUSED(threads);
    Q_UNUSED(translate);
    Q_UNUSED(settings);
    emit transcriptionFinished(QStringLiteral("Mock transcribed text"), QVariantList());
}

void SttEngineInstance::cancelProcessing()
{
}

void SttEngineInstance::onWorkerModelLoaded(bool success, const QString &error)
{
    Q_UNUSED(success);
    Q_UNUSED(error);
}

void SttEngineInstance::onWorkerProgress(int percent)
{
    Q_UNUSED(percent);
}

void SttEngineInstance::onWorkerFinished(const QString &text, const QVariantList &segments)
{
    Q_UNUSED(text);
    Q_UNUSED(segments);
}

void SttEngineInstance::onWorkerError(const QString &error)
{
    Q_UNUSED(error);
}

} // namespace LAStudio
