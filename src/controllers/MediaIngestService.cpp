#include "MediaIngestService.h"

#include "core/PathUtils.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSaveFile>
#include <QtMath>

namespace LAStudio {

MediaIngestService::MediaIngestService(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &MediaIngestService::onProcessFinished);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, &MediaIngestService::onReadyReadStandardError);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        if (m_stage == Stage::Probe) m_probeOutput += m_process.readAllStandardOutput();
    });
}

QString MediaIngestService::ffmpegPath() const
{
    const QString configured = qEnvironmentVariable("LASTUDIO_FFMPEG");
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) return configured;
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

QString MediaIngestService::ffprobePath() const
{
    const QString configured = qEnvironmentVariable("LASTUDIO_FFPROBE");
    if (!configured.isEmpty() && QFileInfo(configured).isFile()) return configured;
    const QString ffmpeg = ffmpegPath();
    if (!ffmpeg.isEmpty()) {
        const QFileInfo info(ffmpeg);
        const QString sibling = info.absolutePath() + QLatin1Char('/') + QStringLiteral("ffprobe")
#ifdef Q_OS_WIN
            + QStringLiteral(".exe")
#endif
            ;
        if (QFileInfo(sibling).isFile()) return sibling;
    }
    return QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
}

bool MediaIngestService::available() const
{
    return !ffmpegPath().isEmpty() && !ffprobePath().isEmpty();
}

void MediaIngestService::ingest(const QString &path)
{
    cancel();
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        fail(QStringLiteral("Media file does not exist: %1").arg(path));
        return;
    }
    if (!available()) {
        fail(QStringLiteral("FFmpeg and FFprobe are required for media import."));
        return;
    }

    QFile input(info.absoluteFilePath());
    if (!input.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("Cannot read media file: %1").arg(input.errorString()));
        return;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!input.atEnd()) {
        const QByteArray chunk = input.read(1024 * 1024);
        if (chunk.isEmpty() && input.error() != QFileDevice::NoError) {
            fail(QStringLiteral("Cannot hash media file: %1").arg(input.errorString()));
            return;
        }
        hash.addData(chunk);
    }
    m_inputPath = info.absoluteFilePath();
    m_hash = QString::fromLatin1(hash.result().toHex());
    m_workspace = PathUtils::cacheDir() + QStringLiteral("/dubbing/imports/") + m_hash;
    m_masterPath = m_workspace + QStringLiteral("/master.wav");
    m_analysisPath = m_workspace + QStringLiteral("/analysis.wav");
    m_manifest.clear();
    m_probeOutput.clear();
    m_stderr.clear();
    QDir().mkpath(m_workspace);
    startProbe();
}

void MediaIngestService::startProbe()
{
    m_stage = Stage::Probe;
    m_process.setProgram(ffprobePath());
    m_process.setArguments({QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-print_format"), QStringLiteral("json"),
                            QStringLiteral("-show_format"), QStringLiteral("-show_streams"), m_inputPath});
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.start();
    if (!m_process.waitForStarted(5000)) fail(QStringLiteral("Could not start FFprobe: %1").arg(m_process.errorString()));
    emit progress(5);
}

void MediaIngestService::startMaster()
{
    m_stage = Stage::Master;
    m_stderr.clear();
    m_process.setProgram(ffmpegPath());
    m_process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                            QStringLiteral("-i"), m_inputPath, QStringLiteral("-map"), QStringLiteral("0:a:0"),
                            QStringLiteral("-vn"), QStringLiteral("-ac"), QStringLiteral("2"), QStringLiteral("-ar"), QStringLiteral("48000"),
                            QStringLiteral("-c:a"), QStringLiteral("pcm_f32le"), m_masterPath});
    m_process.start();
    if (!m_process.waitForStarted(5000)) fail(QStringLiteral("Could not start FFmpeg: %1").arg(m_process.errorString()));
    emit progress(35);
}

void MediaIngestService::startAnalysis()
{
    m_stage = Stage::Analysis;
    m_stderr.clear();
    m_process.setProgram(ffmpegPath());
    m_process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-y"),
                            QStringLiteral("-i"), m_inputPath, QStringLiteral("-map"), QStringLiteral("0:a:0"),
                            QStringLiteral("-vn"), QStringLiteral("-ac"), QStringLiteral("1"), QStringLiteral("-ar"), QStringLiteral("16000"),
                            QStringLiteral("-c:a"), QStringLiteral("pcm_s16le"), m_analysisPath});
    m_process.start();
    if (!m_process.waitForStarted(5000)) fail(QStringLiteral("Could not start FFmpeg: %1").arg(m_process.errorString()));
    emit progress(65);
}

void MediaIngestService::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool ok = status == QProcess::NormalExit && exitCode == 0;
    if (!ok) {
        fail(QStringLiteral("Media import failed during %1: %2")
             .arg(m_stage == Stage::Probe ? QStringLiteral("probe") : QStringLiteral("audio normalization"),
                  QString::fromLocal8Bit(m_stderr).trimmed()));
        return;
    }
    if (m_stage == Stage::Probe) {
        m_probeOutput += m_process.readAllStandardOutput();
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(m_probeOutput, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            fail(QStringLiteral("FFprobe returned invalid JSON: %1").arg(parseError.errorString()));
            return;
        }
        const QJsonObject root = doc.object();
        const QJsonObject format = root.value(QStringLiteral("format")).toObject();
        const double duration = format.value(QStringLiteral("duration")).toString().toDouble();
        const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
        bool video = false;
        bool audio = false;
        int sampleRate = 0;
        int channels = 0;
        for (const QJsonValue &value : streams) {
            const QJsonObject stream = value.toObject();
            const QString type = stream.value(QStringLiteral("codec_type")).toString();
            video = video || type == QStringLiteral("video");
            audio = audio || type == QStringLiteral("audio");
            if (type == QStringLiteral("audio") && sampleRate == 0) {
                sampleRate = stream.value(QStringLiteral("sample_rate")).toString().toInt();
                channels = stream.value(QStringLiteral("channels")).toInt();
            }
        }
        if (streams.isEmpty() || !streams.at(0).isObject() || !audio) {
            fail(QStringLiteral("Media contains no readable audio stream."));
            return;
        }
        m_manifest.insert(QStringLiteral("sourcePath"), m_inputPath);
        m_manifest.insert(QStringLiteral("sourceHash"), QStringLiteral("sha256:") + m_hash);
        m_manifest.insert(QStringLiteral("sourceDurationMs"), qRound64(duration * 1000.0));
        m_manifest.insert(QStringLiteral("sourceSampleRate"), sampleRate);
        m_manifest.insert(QStringLiteral("sourceChannels"), channels);
        m_manifest.insert(QStringLiteral("sourceIsVideo"), video);
        m_manifest.insert(QStringLiteral("workspacePath"), m_workspace);
        const auto finishCached = [this]() {
            m_manifest.insert(QStringLiteral("manifestVersion"), 1);
            QSaveFile file(m_workspace + QStringLiteral("/manifest.json"));
            if (!file.open(QIODevice::WriteOnly)
                || file.write(QJsonDocument::fromVariant(m_manifest).toJson(QJsonDocument::Indented)) < 0
                || !file.commit()) {
                fail(QStringLiteral("Cannot write normalized media manifest."));
                return;
            }
            emit progress(100);
            emit finished(true, m_manifest, QString());
            m_stage = Stage::None;
        };
        if (QFileInfo::exists(m_masterPath) && QFileInfo::exists(m_analysisPath)) {
            m_manifest.insert(QStringLiteral("masterAudioPath"), m_masterPath);
            m_manifest.insert(QStringLiteral("analysisAudioPath"), m_analysisPath);
            finishCached();
            return;
        }
        startMaster();
    } else if (m_stage == Stage::Master) {
        if (!QFileInfo::exists(m_masterPath)) { fail(QStringLiteral("FFmpeg did not create master audio.")); return; }
        startAnalysis();
    } else if (m_stage == Stage::Analysis) {
        if (!QFileInfo::exists(m_analysisPath)) { fail(QStringLiteral("FFmpeg did not create analysis audio.")); return; }
        m_manifest.insert(QStringLiteral("masterAudioPath"), m_masterPath);
        m_manifest.insert(QStringLiteral("analysisAudioPath"), m_analysisPath);
        QSaveFile file(m_workspace + QStringLiteral("/manifest.json"));
        if (!file.open(QIODevice::WriteOnly)
            || file.write(QJsonDocument::fromVariant(m_manifest).toJson(QJsonDocument::Indented)) < 0
            || !file.commit()) {
            fail(QStringLiteral("Cannot write normalized media manifest."));
            return;
        }
        emit progress(100);
        emit finished(true, m_manifest, QString());
        m_stage = Stage::None;
    }
}

void MediaIngestService::onReadyReadStandardError()
{
    m_stderr += m_process.readAllStandardError();
    if (m_stderr.size() > 1024 * 1024) m_stderr = m_stderr.right(1024 * 1024);
}

void MediaIngestService::fail(const QString &error)
{
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
    m_stage = Stage::None;
    emit progress(0);
    emit finished(false, {}, error);
}

void MediaIngestService::cancel()
{
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
    m_stage = Stage::None;
}

} // namespace LAStudio
