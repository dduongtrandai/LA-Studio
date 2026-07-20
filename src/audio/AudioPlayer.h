#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqml.h>

namespace LAStudio {

class AudioPlaybackSession;

class AudioPlayer : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AudioPlayer is managed by AppController")

    // `playing` means this player owns an active playback session. It remains true
    // while paused so QML can retain the active clip and offer Resume.
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY pausedChanged)
    Q_PROPERTY(qint64 playbackPositionMs READ playbackPositionMs NOTIFY playbackPositionChanged)
    Q_PROPERTY(qint64 playbackDurationMs READ playbackDurationMs NOTIFY playbackDurationChanged)

public:
    explicit AudioPlayer(QObject *parent = nullptr);

    bool isPlaying() const { return m_playing; }
    bool isPaused() const { return m_paused; }
    qint64 playbackPositionMs() const;
    qint64 playbackDurationMs() const { return m_playbackDurationMs; }

    Q_INVOKABLE void playPcm(const QByteArray &pcm16Data, int sampleRate);
    Q_INVOKABLE bool playFile(const QString &path);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void stop();

signals:
    void playingChanged();
    void pausedChanged();
    void playbackPositionChanged();
    void playbackDurationChanged();
    void playbackFinished();
    void errorOccurred(const QString &message);

private:
    void updatePlaybackPosition();
    void resetPlaybackState();

    QPointer<AudioPlaybackSession> m_session;
    QTimer m_positionTimer;
    bool m_playing = false;
    bool m_paused = false;
    qint64 m_playbackDurationMs = 0;
};

} // namespace LAStudio
