#pragma once

#include <QObject>
#include <QByteArray>
#include <QBuffer>
#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QtQml/qqml.h>

namespace LAStudio {

class WasapiLoopbackThread;

class AudioRecorder : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AudioRecorder is managed by AppController")

    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(float level READ level NOTIFY levelChanged)
    Q_PROPERTY(bool recordSystemAudio READ recordSystemAudio WRITE setRecordSystemAudio NOTIFY recordSystemAudioChanged)
    Q_PROPERTY(bool saving READ isSaving NOTIFY savingChanged)

public:
    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder() override;

    bool isRecording() const { return m_recording; }
    float level() const { return m_level; }
    bool isSaving() const { return m_saving; }

    bool recordSystemAudio() const { return m_recordSystemAudio; }
    void setRecordSystemAudio(bool val) {
        if (m_recordSystemAudio != val) {
            m_recordSystemAudio = val;
            emit recordSystemAudioChanged();
        }
    }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE QString saveLastRecordingToCache(int targetSampleRate = 24000);
    Q_INVOKABLE void saveLastRecordingToCacheAsync(int targetSampleRate = 24000);

    QByteArray lastRecording() const { return m_buffer; }

signals:
    void recordingChanged();
    void levelChanged();
    void recordSystemAudioChanged();
    void savingChanged();
    void recordingSaved(const QString &path);
    void finished(const QByteArray &pcmData);

private slots:
    void onReadyRead();

private:
    QAudioFormat makeFormat() const;

    QScopedPointer<QAudioSource> m_source;
    QIODevice *m_ioDevice = nullptr;
    QByteArray m_buffer;
    int m_bufferSampleRate = 48000;
    bool m_recording = false;
    float m_level = 0.0f;
    bool m_saving = false;

#ifdef Q_OS_WIN
    QScopedPointer<WasapiLoopbackThread> m_loopbackThread;
#endif
    bool m_recordSystemAudio = false;
};

} // namespace LAStudio


