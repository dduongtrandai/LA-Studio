#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace LAStudio {

class WavIO {
public:
    struct WavData {
        QVector<float> samples;
        int sampleRate = 0;
        int channels = 0;
    };

    static WavData loadAsFloat(const QString &path);
    static WavData loadAsFloatMono16k(const QString &path);
    static WavData loadAsFloatMono24k(const QString &path);

    static bool saveFloat(const QString &path, const float *samples,
                          int numSamples, int sampleRate, int channels = 1);
    static bool savePcm16(const QString &path, const int16_t *samples,
                          int numSamples, int sampleRate, int channels = 1);
};

} // namespace LAStudio

