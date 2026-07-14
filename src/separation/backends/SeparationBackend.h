#pragma once

#include "SeparationTypes.h"
#include "SeparationAudioIO.h"
#include <QString>
#include <QVector>
#include <QList>
#include <functional>

namespace LAStudio {

class SeparationBackend {
public:
    virtual ~SeparationBackend() = default;
    virtual QString id() const = 0;
    
    struct BackendStem {
        QString id;
        QVector<QVector<float>> channels;
    };
    
    struct BackendResult {
        bool success = false;
        QList<BackendStem> stems;
        int sampleRate = 0;
        QString error;
    };
    
    using ProgressCallback = std::function<void(int percent, const QString &stage)>;
    
    virtual BackendResult separate(
        const DecodedAudio &audio,
        const SeparationConfiguration &configuration,
        int numThreads,
        const CancellationToken &cancellation,
        ProgressCallback progress) = 0;
};

} // namespace LAStudio
