#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QAudioDecoder>
#include <QAudioBuffer>

namespace LAStudio {

class SttAudioDecoder : public QObject {
    Q_OBJECT
public:
    explicit SttAudioDecoder(QObject *parent = nullptr);
    ~SttAudioDecoder() override = default;

    void startDecode(const QString &filePath);

signals:
    void finished(const QVector<float> &samples);
    void errorOccurred(const QString &error);

private slots:
    void handleBufferReady();
    void handleFinished();
    void handleError(QAudioDecoder::Error error);

private:
    QAudioDecoder m_decoder;
    QVector<float> m_decodedSamples;
    QString m_filePath;
};

} // namespace LAStudio
