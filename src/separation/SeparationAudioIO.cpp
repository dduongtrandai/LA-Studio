#include "SeparationAudioIO.h"
#include "audio/AudioFileDecoder.h"
#include "audio/WavIO.h"

#include <QFile>

namespace LAStudio {

DecodedAudio SeparationAudioIO::decode(const QString &sourcePath, const QString &tempDir)
{
    Q_UNUSED(tempDir);
    DecodedAudio result;
    WavIO::WavData wav = AudioFileDecoder::decode(sourcePath);
    if (wav.samples.isEmpty() || wav.channels <= 0 || wav.sampleRate <= 0) {
        return result;
    }
    
    result.sampleRate = wav.sampleRate;
    result.channels.resize(wav.channels);
    const int samplesPerChannel = wav.samples.size() / wav.channels;
    for (int c = 0; c < wav.channels; ++c) {
        result.channels[c].resize(samplesPerChannel);
        for (int i = 0; i < samplesPerChannel; ++i) {
            result.channels[c][i] = wav.samples[i * wav.channels + c];
        }
    }
    return result;
}

bool SeparationAudioIO::saveStem(const QString &path, const QVector<QVector<float>> &channels, int sampleRate)
{
    if (channels.isEmpty() || channels[0].isEmpty()) return false;
    const int n = channels[0].size();
    QVector<float> interleaved;
    interleaved.resize(n * channels.size());
    for (int i = 0; i < n; ++i) {
        for (int c = 0; c < channels.size(); ++c) {
            interleaved[i * channels.size() + c] = channels[c][i];
        }
    }
    
    QString stagingPath = path + QStringLiteral(".staging");
    QFile::remove(stagingPath);
    if (!WavIO::saveFloat(stagingPath, interleaved.constData(), interleaved.size(), sampleRate, channels.size())) {
        return false;
    }
    
    QFile::remove(path);
    if (!QFile::rename(stagingPath, path)) {
        QFile::remove(stagingPath);
        return false;
    }
    return true;
}

} // namespace LAStudio
