#include "SherpaOnnxSeparationBackend.h"
#include "SherpaOnnxRuntime.h"

namespace LAStudio {

SeparationBackend::BackendResult SherpaOnnxSeparationBackend::separate(
    const DecodedAudio &audio,
    const SeparationConfiguration &configuration,
    int numThreads,
    const CancellationToken &cancellation,
    ProgressCallback progress)
{
    BackendResult result;
    if (cancellation.isCancelled()) {
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    if (progress) progress(10, QStringLiteral("Loading runtime library"));
    SherpaOnnxRuntime runtime(configuration.runtimePath);
    if (!runtime.isLoaded()) {
        result.error = runtime.errorString();
        return result;
    }

    if (cancellation.isCancelled()) {
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    if (progress) progress(20, QStringLiteral("Configuring models"));
    SherpaOnnxRuntime::Config config{};
    QByteArray modelBytes, vocalsBytes, accompanimentBytes;
    
    if (configuration.pipelineProfile == QStringLiteral("uvr-2stems")) {
        QString modelPath = configuration.modelFilesByRole.value(QStringLiteral("model"));
        modelBytes = modelPath.toLocal8Bit();
        config.model.uvr.model = modelBytes.constData();
    } else if (configuration.pipelineProfile == QStringLiteral("spleeter-2stems")) {
        QString vocalsPath = configuration.modelFilesByRole.value(QStringLiteral("vocals-model"));
        QString accPath = configuration.modelFilesByRole.value(QStringLiteral("accompaniment-model"));
        vocalsBytes = vocalsPath.toLocal8Bit();
        accompanimentBytes = accPath.toLocal8Bit();
        config.model.spleeter.vocals = vocalsBytes.constData();
        config.model.spleeter.accompaniment = accompanimentBytes.constData();
    } else {
        result.error = QStringLiteral("Unsupported pipeline profile: %1").arg(configuration.pipelineProfile);
        return result;
    }

    config.model.numThreads = numThreads > 0 ? numThreads : 1;
    config.model.provider = "cpu";

    if (progress) progress(30, QStringLiteral("Initializing engine"));
    const auto *engine = runtime.createEngine(config);
    if (!engine) {
        result.error = QStringLiteral("sherpa-onnx failed to create source-separation engine.");
        return result;
    }

    if (cancellation.isCancelled()) {
        runtime.destroyEngine(engine);
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    if (progress) progress(40, QStringLiteral("Running inference"));
    QVector<const float *> pointers;
    pointers.reserve(audio.channels.size());
    for (const auto &channel : audio.channels) {
        pointers.push_back(channel.constData());
    }

    const auto *output = runtime.process(engine, pointers.constData(), audio.channels.size(), audio.channels[0].size(), audio.sampleRate);
    if (!output) {
        runtime.destroyEngine(engine);
        result.error = QStringLiteral("sherpa-onnx source separation failed.");
        return result;
    }

    if (cancellation.isCancelled()) {
        runtime.destroyOutput(output);
        runtime.destroyEngine(engine);
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    if (progress) progress(90, QStringLiteral("Mapping output stems"));
    result.sampleRate = output->sampleRate;
    for (int s = 0; s < output->numStems; ++s) {
        BackendStem stem;
        stem.id = (s == 0) ? QStringLiteral("vocals") : QStringLiteral("background");
        for (int c = 0; c < output->stems[s].numChannels; ++c) {
            stem.channels.append(QVector<float>(output->stems[s].samples[c], output->stems[s].samples[c] + output->stems[s].n));
        }
        result.stems.append(std::move(stem));
    }

    runtime.destroyOutput(output);
    runtime.destroyEngine(engine);
    
    result.success = true;
    return result;
}

} // namespace LAStudio
