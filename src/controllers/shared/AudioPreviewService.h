#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QAudioDecoder>
#include <QVector>
#include <QtQml/qqml.h>

namespace LAStudio {

class TtsEngine;
class AudioPlayer;
class WaveformProvider;

class AudioPreviewService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("AudioPreviewService is managed by AppController")

    Q_PROPERTY(QVariantList wavSamples READ wavSamples NOTIFY wavSamplesChanged)
    Q_PROPERTY(QString wavSamplesSourcePath READ wavSamplesSourcePath NOTIFY wavSamplesChanged)
    Q_PROPERTY(bool wavSamplesLoading READ wavSamplesLoading NOTIFY wavSamplesLoadingChanged)

public:
    explicit AudioPreviewService(TtsEngine* tts, AudioPlayer* player, WaveformProvider* waveformProvider, QObject *parent = nullptr);
    ~AudioPreviewService() override = default;

    QVariantList wavSamples() const { return m_wavSamples; }
    QString wavSamplesSourcePath() const { return m_wavSamplesSourcePath; }
    bool wavSamplesLoading() const { return m_wavSamplesLoading; }

    Q_INVOKABLE void requestWavSamples(const QString &path);
    Q_INVOKABLE void saveWav(const QString &path);
    Q_INVOKABLE void playLastTts();
    Q_INVOKABLE void playLastTtsAtPosition(qint64 positionMs);

signals:
    void wavSamplesChanged();
    void wavSamplesLoadingChanged();
    void errorOccurred(const QString &msg);

private:
    void publishDecodedSamples(const QString &sourcePath, quint64 requestId);

private slots:
    void handleDecoderBufferReady();
    void handleDecoderFinished();
    void handleDecoderError(QAudioDecoder::Error error);

private:
    TtsEngine* m_tts = nullptr;
    AudioPlayer* m_player = nullptr;
    WaveformProvider* m_waveformProvider = nullptr;

    QVariantList m_wavSamples;
    QString m_wavSamplesSourcePath;
    bool m_wavSamplesLoading = false;
    quint64 m_wavSamplesRequestId = 0;
    QAudioDecoder m_decoder;
    QVector<float> m_decodedSamples;
    QString m_decoderSourcePath;
    quint64 m_decoderRequestId = 0;
};

} // namespace LAStudio
