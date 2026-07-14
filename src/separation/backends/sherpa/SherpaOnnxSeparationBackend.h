#pragma once

#include "backends/SeparationBackend.h"

namespace LAStudio {

class SherpaOnnxSeparationBackend final : public SeparationBackend {
public:
    SherpaOnnxSeparationBackend() = default;
    ~SherpaOnnxSeparationBackend() override = default;
    
    QString id() const override { return QStringLiteral("sherpa-onnx"); }
    
    BackendResult separate(
        const DecodedAudio &audio,
        const SeparationConfiguration &configuration,
        int numThreads,
        const CancellationToken &cancellation,
        ProgressCallback progress) override;
};

} // namespace LAStudio
