#pragma once
#include <QObject>
#include <QString>
#include <QVector>

namespace LAStudio {

// Asynchronous adapter for the shared audio input decoder.  Kept under the
// STT controller namespace for source compatibility with existing callers;
// the actual container decoding and normalization lives in AudioFileDecoder
// so Alignment and API transcription receive the exact same input policy.
class SttAudioDecoder : public QObject {
    Q_OBJECT
public:
    explicit SttAudioDecoder(QObject *parent = nullptr);
    ~SttAudioDecoder() override = default;

    void startDecode(const QString &filePath);

signals:
    void finished(const QVector<float> &samples);
    void errorOccurred(const QString &error);

private:
    quint64 m_decodeRequestId = 0;
};

} // namespace LAStudio
