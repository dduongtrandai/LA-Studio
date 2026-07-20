#include "audio/AudioPlayer.h"

namespace LAStudio {

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
}

qint64 AudioPlayer::playbackPositionMs() const
{
    return 0;
}

void AudioPlayer::playPcm(const QByteArray &, int)
{
}

bool AudioPlayer::playFile(const QString &)
{
    return false;
}

void AudioPlayer::pause()
{
}

void AudioPlayer::resume()
{
}

void AudioPlayer::seek(qint64)
{
}

void AudioPlayer::stop()
{
}

} // namespace LAStudio
