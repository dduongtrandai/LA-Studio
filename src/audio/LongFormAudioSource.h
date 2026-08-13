#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace LAStudio {

// Disk-backed reader used by long-form STT.  It deliberately returns only a
// bounded PCM window so an hours-long input never becomes one giant QVector.
class LongFormAudioSource final
{
public:
    static bool probeDurationMs(const QString &path, qint64 &durationMs,
                                QString *error = nullptr);

    // Materializes a source into a deterministic, mono 16 kHz float32 cache.
    // The cache is invalidated automatically when the source size/mtime
    // changes.  The returned path is safe for random range reads.
    static bool prepareMono16k(const QString &sourcePath,
                               const QString &cacheRoot,
                               QString &normalizedPath,
                               qint64 &durationMs,
                               QString *error = nullptr);

    static QVector<float> readMono16kRange(const QString &path,
                                           qint64 startMs,
                                           qint64 durationMs,
                                           QString *error = nullptr);

    static QString ffmpegExecutable();
    static QString ffprobeExecutable();
};

} // namespace LAStudio
