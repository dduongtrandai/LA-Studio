#include "controllers/dubbing/DubbingExportJob.h"

#include "dubbing/AudioTimelineMixer.h"
#include "dubbing/media/AtomicMediaCommit.h"
#include "dubbing/media/MediaToolService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrent>

namespace LAStudio {

namespace {
QString srtTime(qint64 value)
{
    value = qMax<qint64>(0, value);
    const qint64 hours = value / 3600000;
    const qint64 minutes = (value / 60000) % 60;
    const qint64 seconds = (value / 1000) % 60;
    const qint64 millis = value % 1000;
    return QStringLiteral("%1:%2:%3,%4")
        .arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0')).arg(millis, 3, 10, QLatin1Char('0'));
}

bool writeTargetSubtitles(const QVariantList &segments, const QString &path)
{
    QStringList lines;
    int cue = 1;
    for (const QVariant &value : segments) {
        const QVariantMap segment = value.toMap();
        const QString text = segment.value(QStringLiteral("targetText")).toString().trimmed();
        const qint64 start = segment.value(QStringLiteral("startMs")).toLongLong();
        const qint64 end = segment.value(QStringLiteral("endMs")).toLongLong();
        if (text.isEmpty() || end <= start) continue;
        lines.append(QString::number(cue++));
        lines.append(QStringLiteral("%1 --> %2").arg(srtTime(start), srtTime(end)));
        lines.append(text);
        lines.append(QString());
    }
    if (lines.isEmpty()) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(lines.join(QLatin1Char('\n')).toUtf8()) >= 0
        && file.commit();
}
}

DubbingExportJob::DubbingExportJob(QObject *parent)
    : QObject(parent)
{
    m_renderWatcher = new QFutureWatcher<QVariantMap>(this);
    connect(m_renderWatcher, &QFutureWatcher<QVariantMap>::finished,
            this, &DubbingExportJob::onRenderFinished);
    m_mediaTools = new MediaToolService(this);
    connect(m_mediaTools, &MediaToolService::finished,
            this, &DubbingExportJob::onMediaFinished);
}

DubbingExportJob::~DubbingExportJob()
{
    cancel();
    if (m_renderWatcher) {
        m_renderWatcher->cancel();
        m_renderWatcher->waitForFinished();
    }
}

bool DubbingExportJob::renderPreview(const QVariantList &segments, const QString &projectPath,
                                     const QString &backgroundPath, const QString &path)
{
    if (m_running) { fail(QStringLiteral("Finish the active export operation first.")); return false; }
    QString outputPath = path;
    if (outputPath.isEmpty()) {
        if (projectPath.isEmpty()) { fail(QStringLiteral("Save the project before rendering a preview.")); return false; }
        outputPath = QFileInfo(projectPath).absolutePath() + QStringLiteral("/preview.wav");
    }
    if (!m_renderWatcher || m_renderWatcher->isRunning()) { fail(QStringLiteral("An audio mix is already running.")); return false; }
    m_running = true;
    m_renderStagingPath = outputPath + QStringLiteral(".workflow-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".staging");
    m_renderCancel = std::make_shared<QAtomicInteger<bool>>(false);
    const auto cancel = m_renderCancel;
    const QString stagingPath = m_renderStagingPath;
    emit progressChanged(QStringLiteral("mix"), 0);
    m_renderWatcher->setFuture(QtConcurrent::run(
        [segments, outputPath, stagingPath, backgroundPath, cancel]() {
        QString error;
        const bool mixed = AudioTimelineMixer::mixSegments(
            segments, stagingPath, backgroundPath, &error, cancel.get());
        const bool committed = mixed && !cancel->loadAcquire()
            && AtomicMediaCommit::commit(stagingPath, outputPath, &error);
        QFile::remove(stagingPath);
        return QVariantMap{{QStringLiteral("success"), committed},
                           {QStringLiteral("outputPath"), outputPath},
                           {QStringLiteral("error"), error}};
    }));
    return true;
}

bool DubbingExportJob::startExport(const QString &sourceMediaPath, const QString &audioPath,
                                   const QString &outputPath, const QVariantList &segments)
{
    if (m_running) { fail(QStringLiteral("Finish the active export operation first.")); return false; }
    if (outputPath.isEmpty()) { fail(QStringLiteral("Choose an output path.")); return false; }
    if (sourceMediaPath.isEmpty()) { fail(QStringLiteral("Import source media before exporting.")); return false; }
    if (audioPath.isEmpty() || !QFileInfo::exists(audioPath)) {
        fail(QStringLiteral("Generate and render audio preview before exporting.")); return false;
    }
    m_exportDestination = outputPath;
    m_exportAudioPath = audioPath;
    const QString suffix = QFileInfo(sourceMediaPath).suffix().toLower();
    const bool video = suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mkv")
        || suffix == QStringLiteral("mov") || suffix == QStringLiteral("webm")
        || suffix == QStringLiteral("avi");
    const QFileInfo destinationInfo(outputPath);
    const QString stagingSuffix = destinationInfo.suffix().isEmpty()
        ? QStringLiteral(".staging") : QStringLiteral(".") + destinationInfo.suffix();
    m_exportStagingPath = outputPath + QStringLiteral(".workflow-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + stagingSuffix;
    if (!video) {
        if (!QFile::copy(m_exportAudioPath, m_exportStagingPath)) {
            clearExportPaths();
            fail(QStringLiteral("Failed to stage rendered WAV for export: %1").arg(m_exportStagingPath));
            return false;
        }
        QString error;
        if (!AtomicMediaCommit::commit(m_exportStagingPath, outputPath, &error)) {
            QFile::remove(m_exportStagingPath);
            clearExportPaths();
            fail(error);
            return false;
        }
        QFile::remove(m_exportStagingPath);
        clearExportPaths();
        emit exported(outputPath);
        return true;
    }
    if (!m_mediaTools) { clearExportPaths(); fail(QStringLiteral("Media tool service is unavailable.")); return false; }
    m_running = true;
    emit progressChanged(QStringLiteral("export"), 0);
    m_exportSubtitlePath = m_exportStagingPath + QStringLiteral(".srt");
    if (!writeTargetSubtitles(segments, m_exportSubtitlePath))
        m_exportSubtitlePath.clear();
    m_mediaTools->muxVideoWithAudio(sourceMediaPath, m_exportAudioPath,
                                    m_exportSubtitlePath, m_exportStagingPath);
    return true;
}

void DubbingExportJob::cancel()
{
    if (!m_running) return;
    if (m_renderCancel) m_renderCancel->storeRelease(true);
    if (m_mediaTools) m_mediaTools->cancel();
    QFile::remove(m_renderStagingPath);
    QFile::remove(m_exportStagingPath);
    QFile::remove(m_exportSubtitlePath);
    m_running = false;
    clearExportPaths();
}

void DubbingExportJob::onRenderFinished()
{
    const QVariantMap result = m_renderWatcher->result();
    QFile::remove(m_renderStagingPath);
    m_renderStagingPath.clear();
    m_renderCancel.reset();
    m_running = false;
    if (!result.value(QStringLiteral("success")).toBool()) {
        fail(result.value(QStringLiteral("error"), QStringLiteral("Audio mix failed.")).toString());
        return;
    }
    const QString outputPath = result.value(QStringLiteral("outputPath")).toString();
    if (!QFileInfo::exists(outputPath)) { fail(QStringLiteral("Audio mix completed without an output file.")); return; }
    emit progressChanged(QStringLiteral("mix"), 100);
    emit previewReady(outputPath);
}

void DubbingExportJob::onMediaFinished(bool success, const QString &outputPath, const QString &error)
{
    if (!m_running) return;
    if (!success || outputPath != m_exportStagingPath || !QFileInfo::exists(m_exportStagingPath)) {
        QFile::remove(m_exportStagingPath);
        QFile::remove(m_exportSubtitlePath);
        clearExportPaths();
        m_running = false;
        fail(error.isEmpty() ? QStringLiteral("Media export failed.") : error);
        return;
    }
    QString commitError;
    if (!AtomicMediaCommit::commit(m_exportStagingPath, m_exportDestination, &commitError)) {
        QFile::remove(m_exportStagingPath);
        QFile::remove(m_exportSubtitlePath);
        clearExportPaths();
        m_running = false;
        fail(commitError);
        return;
    }
    QFile::remove(m_exportStagingPath);
    QFile::remove(m_exportSubtitlePath);
    const QString destination = m_exportDestination;
    clearExportPaths();
    m_running = false;
    emit progressChanged(QStringLiteral("export"), 100);
    emit exported(destination);
}

void DubbingExportJob::clearExportPaths()
{
    m_exportDestination.clear();
    m_exportStagingPath.clear();
    m_exportAudioPath.clear();
    m_exportSubtitlePath.clear();
}

void DubbingExportJob::fail(const QString &message)
{
    m_running = false;
    emit failed(message);
}

} // namespace LAStudio
