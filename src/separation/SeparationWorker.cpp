#include "SeparationWorker.h"
#include "SeparationAudioIO.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QCryptographicHash>
#include <QDateTime>
#include <QUrl>

namespace LAStudio {

static QString computeSourceHash(const QString &sourcePath)
{
    QFileInfo info(sourcePath);
    QString stringToHash = QStringLiteral("%1:%2:%3").arg(info.absoluteFilePath()).arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
    return QString::fromUtf8(QCryptographicHash::hash(stringToHash.toUtf8(), QCryptographicHash::Md5).toHex());
}

SeparationWorker::SeparationWorker(std::shared_ptr<SeparationBackendFactory> factory, QObject *parent)
    : QObject(parent)
    , m_factory(factory)
{
}

void SeparationWorker::process(const SeparationRequest &request, QAtomicInt *cancelFlag)
{
    CancellationToken cancellation;
    if (cancelFlag) {
        cancellation.isCancelled = [cancelFlag]() {
            return cancelFlag->loadAcquire() != 0;
        };
    }

    auto checkCancelled = [&]() -> bool {
        return cancellation.isCancelled();
    };

    SeparationResult result;
    result.sourceHash = computeSourceHash(request.sourcePath);

    if (request.sourcePath.isEmpty() || !QFileInfo::exists(request.sourcePath)) {
        result.success = false;
        result.errorCode = SeparationErrorCode::InvalidRequest;
        result.error = QStringLiteral("Source audio file does not exist.");
        emit finished(result);
        return;
    }

    if (request.configuration.backendId.isEmpty()) {
        result.success = false;
        result.errorCode = SeparationErrorCode::InvalidRequest;
        result.error = QStringLiteral("Backend ID is not specified.");
        emit finished(result);
        return;
    }

    if (checkCancelled()) {
        result.success = false;
        result.errorCode = SeparationErrorCode::Cancelled;
        result.error = QStringLiteral("Source separation cancelled.");
        emit finished(result);
        return;
    }

    // 2. Resolve backend
    if (!m_factory || !m_factory->hasBackend(request.configuration.backendId)) {
        result.success = false;
        result.errorCode = SeparationErrorCode::UnsupportedBackend;
        result.error = QStringLiteral("Unsupported backend: %1").arg(request.configuration.backendId);
        emit finished(result);
        return;
    }

    auto backend = m_factory->createBackend(request.configuration.backendId);
    if (!backend) {
        result.success = false;
        result.errorCode = SeparationErrorCode::UnsupportedBackend;
        result.error = QStringLiteral("Failed to create backend: %1").arg(request.configuration.backendId);
        emit finished(result);
        return;
    }

    // Create target cache dir structure
    QString cacheDirPath = QDir(request.outputRoot).filePath(result.sourceHash);
    QDir().mkpath(cacheDirPath);

    // 3. Decode input audio
    emit progress(5, QStringLiteral("Decoding audio"));
    DecodedAudio decoded = SeparationAudioIO::decode(request.sourcePath, cacheDirPath);
    if (!decoded.isValid()) {
        result.success = false;
        result.errorCode = SeparationErrorCode::DecodeFailed;
        result.error = QStringLiteral("Could not decode the source media audio track. Install the managed FFmpeg runtime for unsupported media formats.");
        emit finished(result);
        return;
    }

    if (checkCancelled()) {
        result.success = false;
        result.errorCode = SeparationErrorCode::Cancelled;
        result.error = QStringLiteral("Source separation cancelled.");
        emit finished(result);
        return;
    }

    // 4. Run inference
    auto progressWrapper = [this](int percent, const QString &stage) {
        emit progress(percent, stage);
    };

    auto backendResult = backend->separate(decoded, request.configuration, request.numThreads, cancellation, progressWrapper);
    
    if (checkCancelled() || backendResult.error == QStringLiteral("Cancelled")) {
        result.success = false;
        result.errorCode = SeparationErrorCode::Cancelled;
        result.error = QStringLiteral("Source separation cancelled.");
        emit finished(result);
        return;
    }

    if (!backendResult.success) {
        result.success = false;
        result.errorCode = SeparationErrorCode::InferenceFailed;
        result.error = backendResult.error;
        emit finished(result);
        return;
    }

    // 5. Save output stems
    emit progress(95, QStringLiteral("Saving separated audio stems"));
    QList<SeparationStem> stems;
    bool saveSucceeded = true;
    for (const auto &bstem : backendResult.stems) {
        SeparationStem stem;
        stem.id = bstem.id;
        stem.path = QDir(cacheDirPath).filePath(bstem.id + QStringLiteral(".wav"));
        stem.sampleRate = backendResult.sampleRate;
        stem.channels = bstem.channels.size();

        if (!SeparationAudioIO::saveStem(stem.path, bstem.channels, backendResult.sampleRate)) {
            saveSucceeded = false;
            break;
        }
        stems.append(stem);
    }

    if (checkCancelled()) {
        // Cleanup partial files
        for (const auto &stem : stems) {
            QFile::remove(stem.path);
        }
        result.success = false;
        result.errorCode = SeparationErrorCode::Cancelled;
        result.error = QStringLiteral("Source separation cancelled.");
        emit finished(result);
        return;
    }

    if (!saveSucceeded) {
        for (const auto &stem : stems) {
            QFile::remove(stem.path);
        }
        result.success = false;
        result.errorCode = SeparationErrorCode::OutputFailed;
        result.error = QStringLiteral("Could not save separated WAV stems.");
        emit finished(result);
        return;
    }

    result.success = true;
    result.stems = stems;

    emit progress(100, QStringLiteral("Finished"));
    emit finished(result);
}

} // namespace LAStudio
