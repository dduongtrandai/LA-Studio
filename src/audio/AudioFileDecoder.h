#pragma once

#include "WavIO.h"

#include <QString>

namespace LAStudio {

// Synchronous decoder for worker-thread audio pipelines. It keeps container
// decoding separate from WAV serialization and provides one fallback policy
// for every backend that accepts user-supplied audio.
class AudioFileDecoder {
public:
    static WavIO::WavData decode(const QString &path, QString *error = nullptr);
    static WavIO::WavData decodeMono(const QString &path,
                                     int targetSampleRate,
                                     QString *error = nullptr);
};

} // namespace LAStudio
