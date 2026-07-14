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

void AudioPlayer::playFile(const QString &)
{
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
