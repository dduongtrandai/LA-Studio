#include "WavIO.h"
#include "core/Logger.h"

#include <QFile>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace LAStudio {

#pragma pack(push, 1)
struct RiffHeader {
    char     riff[4];
    uint32_t chunkSize;
    char     wave[4];
};
struct FmtChunk {
    char     fmt[4];
    uint32_t subchunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};
struct DataChunkHeader {
    char     data[4];
    uint32_t dataSize;
};
#pragma pack(pop)

WavIO::WavData WavIO::loadAsFloat(const QString &path)
{
    WavData result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Logger::error("WavIO", "Cannot open: " + path);
        return result;
    }

    QByteArray raw = file.readAll();
    if (raw.size() < static_cast<qsizetype>(sizeof(RiffHeader) + sizeof(FmtChunk) + sizeof(DataChunkHeader))) {
        Logger::error("WavIO", "File too small: " + path);
        return result;
    }

    auto *riff = reinterpret_cast<const RiffHeader *>(raw.constData());
    if (std::memcmp(riff->riff, "RIFF", 4) != 0 || std::memcmp(riff->wave, "WAVE", 4) != 0) {
        Logger::error("WavIO", "Not a WAV file: " + path);
        return result;
    }

    const char *ptr = raw.constData() + sizeof(RiffHeader);
    const char *end = raw.constData() + raw.size();
    const FmtChunk *fmt = nullptr;
    const char *dataPtr = nullptr;
    uint32_t dataSize = 0;

    while (ptr + 8 <= end) {
        auto chunkId = ptr;
        uint32_t chunkSize;
        std::memcpy(&chunkSize, ptr + 4, 4);

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            fmt = reinterpret_cast<const FmtChunk *>(ptr);
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            dataPtr = ptr + 8;
            dataSize = chunkSize;
        }

        ptr += 8 + chunkSize;
        if (chunkSize % 2 != 0) ptr++; // padding byte
    }

    if (!fmt || !dataPtr) {
        Logger::error("WavIO", "Missing fmt or data chunk: " + path);
        return result;
    }

    result.sampleRate = static_cast<int>(fmt->sampleRate);
    result.channels   = fmt->numChannels;

    int bps = fmt->bitsPerSample;
    size_t numTotalSamples = dataSize / (bps / 8);
    result.samples.resize(static_cast<int>(numTotalSamples));

    if (bps == 16) {
        auto *src = reinterpret_cast<const int16_t *>(dataPtr);
        for (size_t i = 0; i < numTotalSamples; ++i)
            result.samples[static_cast<int>(i)] = static_cast<float>(src[i]) / 32768.0f;
    } else if (bps == 32 && fmt->audioFormat == 3) { // IEEE float
        auto *src = reinterpret_cast<const float *>(dataPtr);
        for (size_t i = 0; i < numTotalSamples; ++i)
            result.samples[static_cast<int>(i)] = src[i];
    } else {
        Logger::error("WavIO", QString("Unsupported bps %1 format %2").arg(bps).arg(fmt->audioFormat));
    }

    return result;
}

WavIO::WavData WavIO::loadAsFloatMono16k(const QString &path)
{
    WavData wav = loadAsFloat(path);
    if (wav.samples.isEmpty()) return wav;

    // Stereo → mono
    if (wav.channels == 2) {
        int monoLen = wav.samples.size() / 2;
        QVector<float> mono(monoLen);
        for (int i = 0; i < monoLen; ++i)
            mono[i] = (wav.samples[i * 2] + wav.samples[i * 2 + 1]) * 0.5f;
        wav.samples = mono;
        wav.channels = 1;
    }

    // Resample to 16kHz via linear interpolation
    if (wav.sampleRate != 16000 && wav.sampleRate > 0) {
        double ratio = 16000.0 / wav.sampleRate;
        int newLen = static_cast<int>(wav.samples.size() * ratio);
        QVector<float> resampled(newLen);
        for (int i = 0; i < newLen; ++i) {
            double srcIdx = i / ratio;
            int idx0 = static_cast<int>(srcIdx);
            int idx1 = std::min(idx0 + 1, static_cast<int>(wav.samples.size() - 1));
            double frac = srcIdx - idx0;
            resampled[i] = static_cast<float>(wav.samples[idx0] * (1.0 - frac) +
                                               wav.samples[idx1] * frac);
        }
        wav.samples = resampled;
        wav.sampleRate = 16000;
    }

    return wav;
}

WavIO::WavData WavIO::loadAsFloatMono24k(const QString &path)
{
    WavData wav = loadAsFloat(path);
    if (wav.samples.isEmpty()) return wav;

    // Stereo → mono
    if (wav.channels == 2) {
        int monoLen = wav.samples.size() / 2;
        QVector<float> mono(monoLen);
        for (int i = 0; i < monoLen; ++i)
            mono[i] = (wav.samples[i * 2] + wav.samples[i * 2 + 1]) * 0.5f;
        wav.samples = mono;
        wav.channels = 1;
    }

    // Resample to 24kHz via linear interpolation
    if (wav.sampleRate != 24000 && wav.sampleRate > 0) {
        double ratio = 24000.0 / wav.sampleRate;
        int newLen = static_cast<int>(wav.samples.size() * ratio);
        QVector<float> resampled(newLen);
        for (int i = 0; i < newLen; ++i) {
            double srcIdx = i / ratio;
            int idx0 = static_cast<int>(srcIdx);
            int idx1 = std::min(idx0 + 1, static_cast<int>(wav.samples.size() - 1));
            double frac = srcIdx - idx0;
            resampled[i] = static_cast<float>(wav.samples[idx0] * (1.0 - frac) +
                                               wav.samples[idx1] * frac);
        }
        wav.samples = resampled;
        wav.sampleRate = 24000;
    }

    return wav;
}

bool WavIO::saveFloat(const QString &path, const float *samples,
                       int numSamples, int sampleRate, int channels)
{
    QVector<int16_t> pcm(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        float s = std::clamp(samples[i], -1.0f, 1.0f);
        pcm[i] = static_cast<int16_t>(s * 32767.0f);
    }
    return savePcm16(path, pcm.constData(), numSamples, sampleRate, channels);
}

bool WavIO::savePcm16(const QString &path, const int16_t *samples,
                       int numSamples, int sampleRate, int channels)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    uint32_t dataSize = static_cast<uint32_t>(numSamples * 2);

    RiffHeader riff;
    std::memcpy(riff.riff, "RIFF", 4);
    riff.chunkSize = 36 + dataSize;
    std::memcpy(riff.wave, "WAVE", 4);

    FmtChunk fmt;
    std::memcpy(fmt.fmt, "fmt ", 4);
    fmt.subchunkSize  = 16;
    fmt.audioFormat    = 1; // PCM
    fmt.numChannels    = static_cast<uint16_t>(channels);
    fmt.sampleRate     = static_cast<uint32_t>(sampleRate);
    fmt.bitsPerSample  = 16;
    fmt.blockAlign     = static_cast<uint16_t>(channels * 2);
    fmt.byteRate       = fmt.sampleRate * fmt.blockAlign;

    DataChunkHeader data;
    std::memcpy(data.data, "data", 4);
    data.dataSize = dataSize;

    file.write(reinterpret_cast<const char *>(&riff), sizeof(riff));
    file.write(reinterpret_cast<const char *>(&fmt), sizeof(fmt));
    file.write(reinterpret_cast<const char *>(&data), sizeof(data));
    file.write(reinterpret_cast<const char *>(samples), dataSize);

    return true;
}

} // namespace LAStudio

