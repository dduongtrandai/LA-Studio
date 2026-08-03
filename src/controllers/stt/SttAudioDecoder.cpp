#include "SttAudioDecoder.h"
#include "audio/AudioFileDecoder.h"
#include "core/PathUtils.h"
#include "core/Logger.h"
#include <QtConcurrent>
#include <QFutureWatcher>

namespace LAStudio {

namespace {

struct DecodeResult {
    QVector<float> samples;
    QString error;
};

} // namespace

SttAudioDecoder::SttAudioDecoder(QObject *parent)
    : QObject(parent)
{
}

void SttAudioDecoder::startDecode(const QString &filePath)
{
    const QString localPath = PathUtils::urlToLocalPath(filePath);
    const quint64 requestId = ++m_decodeRequestId;
    Logger::info(QStringLiteral("SttAudioDecoder"),
                 QStringLiteral("Decoding audio with shared AudioFileDecoder: %1").arg(localPath));

    auto *watcher = new QFutureWatcher<DecodeResult>(this);
    connect(watcher, &QFutureWatcher<DecodeResult>::finished, this,
            [this, watcher, requestId, localPath]() {
        const DecodeResult result = watcher->result();
        watcher->deleteLater();
        if (requestId != m_decodeRequestId)
            return;
        if (result.samples.isEmpty()) {
            emit errorOccurred(result.error.isEmpty()
                                   ? QStringLiteral("No audio data was decoded.")
                                   : result.error);
            return;
        }
        Logger::info(QStringLiteral("SttAudioDecoder"),
                     QStringLiteral("Decoded shared audio input: %1 samples at 16 kHz from %2")
                         .arg(result.samples.size()).arg(localPath));
        emit finished(result.samples);
    });
    watcher->setFuture(QtConcurrent::run([localPath]() {
        DecodeResult result;
        result.samples = AudioFileDecoder::decodeMono(localPath, 16000, &result.error).samples;
        return result;
    }));
}

} // namespace LAStudio
