#pragma once

#include <QString>
#include <QVector>

namespace LAStudio {

struct DecodedAudio {
    QVector<QVector<float>> channels;
    int sampleRate = 0;
    bool isValid() const { return sampleRate > 0 && !channels.isEmpty() && !channels[0].isEmpty(); }
};

class SeparationAudioIO {
public:
    static DecodedAudio decode(const QString &sourcePath, const QString &tempDir);
    static bool saveStem(const QString &path, const QVector<QVector<float>> &channels, int sampleRate);
};

} // namespace LAStudio
