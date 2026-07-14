namespace LAStudio {
class WasapiLoopbackThread {};
}

#include "audio/AudioRecorder.h"
#include <algorithm>

namespace LAStudio {

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
{
    m_buffer.resize(32000);
    std::fill(m_buffer.begin(), m_buffer.end(), 0);
}

AudioRecorder::~AudioRecorder()
{
}

void AudioRecorder::start()
{
}

void AudioRecorder::stop()
{
}

QString AudioRecorder::saveLastRecordingToCache(int)
{
    return QString();
}

void AudioRecorder::saveLastRecordingToCacheAsync(int)
{
}

void AudioRecorder::onReadyRead()
{
}

QAudioFormat AudioRecorder::makeFormat() const
{
    return QAudioFormat();
}

} // namespace LAStudio
