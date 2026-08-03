#include "VoiceIsolatorController.h"
#include "SourceSeparationService.h"
#include "core/PathUtils.h"
#include "controllers/app/AppController.h"
#include "core/ModelManager.h"
#include "core/RuntimeManager.h"
#include "audio/AudioFileDecoder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSaveFile>
#include <QUrl>
#include <QThreadPool>
#include <QCoreApplication>
#include <QPointer>
#include <QDateTime>

namespace LAStudio {

namespace {

const QString kIsolationTempPrefix = QStringLiteral("LA-Studio-VoiceIsolator-");

void removeStaleIsolationTempDirs()
{
    QDir tempRoot(QDir::tempPath());
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    const QFileInfoList entries = tempRoot.entryInfoList(
        {kIsolationTempPrefix + QStringLiteral("*")},
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Time);

    for (const QFileInfo &entry : entries) {
        if (entry.lastModified() < cutoff) {
            QDir(entry.absoluteFilePath()).removeRecursively();
        }
    }
}

}

VoiceIsolatorController::VoiceIsolatorController(QObject *parent)
    : QObject(parent), m_service(new SourceSeparationService(this))
{
    removeStaleIsolationTempDirs();

    m_runtimePath = qEnvironmentVariable("SHERPA_ONNX_RUNTIME");
    m_modelPath = qEnvironmentVariable("SHERPA_ONNX_UVR_MODEL");
    m_spleeterVocalsPath = qEnvironmentVariable("SHERPA_ONNX_SPLEETER_VOCALS");
    m_spleeterAccompanimentPath = qEnvironmentVariable("SHERPA_ONNX_SPLEETER_ACCOMPANIMENT");
#ifdef LASTUDIO_SHERPA_ONNX_ROOT
    const QString devRoot = QStringLiteral(LASTUDIO_SHERPA_ONNX_ROOT);
    if (m_runtimePath.isEmpty()) {
        const QString candidate = QDir(devRoot).filePath(QStringLiteral("bin/sherpa-onnx-c-api.dll"));
        if (QFileInfo(candidate).isFile()) m_runtimePath = candidate;
    }
#endif
    // Separation stems are session-owned temporary files. Do not persist paths
    // to them: they are removed when the session is cleared or the app exits.
    QSettings().remove(QStringLiteral("voiceIsolator/recent"));
    connect(m_service, &SourceSeparationService::progress, this, [this](int value, const QString &stage) {
        Q_UNUSED(stage);
        m_progress = value;
        emit stateChanged();
    });
    connect(m_service, &SourceSeparationService::finished, this, [this](const SeparationResult &result) {
        m_processing = false;
        if (!result.success) {
            m_lastError = result.error;
        } else {
            m_lastError.clear();
            for (const auto &stem : result.stems) {
                if (stem.id == QStringLiteral("vocals")) {
                    m_vocalsPath = stem.path;
                } else if (stem.id == QStringLiteral("background")) {
                    m_backgroundPath = stem.path;
                }
            }
            loadVocalsSamples(m_vocalsPath);
            loadBackgroundSamples(m_backgroundPath);
            QVariantMap manifest;
            manifest.insert(QStringLiteral("vocalsPath"), m_vocalsPath);
            manifest.insert(QStringLiteral("backgroundPath"), m_backgroundPath);
            manifest.insert(QStringLiteral("sourceHash"), result.sourceHash);
            addRecent(manifest);
        }
        emit stateChanged();
    });
}

QString VoiceIsolatorController::runtimePath() const
{
    if (m_hasActiveConfig) {
        return m_activeConfig.runtimePath;
    }
    if (m_configurationOverrideActive) {
        return QFileInfo(m_runtimePath).isFile() ? m_runtimePath : QString();
    }
    if (!m_runtimePath.isEmpty() && QFileInfo(m_runtimePath).isFile()) {
        return m_runtimePath;
    }
    AppController *app = AppController::instance();
    if (app && app->runtimes()) {
        for (const auto &rtVal : app->runtimes()->allRuntimes()) {
            QString rtId = rtVal.toMap().value(QStringLiteral("id")).toString();
            if (rtId == QStringLiteral("sherpa-onnx-win-x86_64-cpu") ||
                rtId == QStringLiteral("sherpa-onnx-source-separation-win-x86_64-cpu")) {
                QString path = app->runtimes()->getRuntimePath(rtId);
                if (!path.isEmpty() && QFileInfo(path).isFile()) {
                    return path;
                }
            }
        }
    }
    return QString();
}

QString VoiceIsolatorController::modelPath() const
{
    if (m_hasActiveConfig) {
        return m_activeConfig.modelFilesByRole.value(QStringLiteral("model"));
    }
    if (m_configurationOverrideActive) {
        return QFileInfo(m_modelPath).isFile() ? m_modelPath : QString();
    }
    if (!m_modelPath.isEmpty() && QFileInfo(m_modelPath).isFile()) {
        return m_modelPath;
    }
    AppController *app = AppController::instance();
    if (app && app->models()) {
        for (const QString &modelId : {QStringLiteral("k2-fsa/sherpa-onnx-uvr-vocals-ft"),
                                      QStringLiteral("k2-fsa/sherpa-onnx-source-separation")}) {
            const QString path = app->models()->filePath(modelId, QStringLiteral("UVR-MDX-NET-Voc_FT.onnx"));
            if (!path.isEmpty() && QFileInfo(path).isFile()) return path;
        }
    }
    return QString();
}

QString VoiceIsolatorController::spleeterVocalsPath() const
{
    if (m_hasActiveConfig) {
        return m_activeConfig.modelFilesByRole.value(QStringLiteral("vocals-model"));
    }
    if (m_configurationOverrideActive) {
        return QFileInfo(m_spleeterVocalsPath).isFile() ? m_spleeterVocalsPath : QString();
    }
    if (!m_spleeterVocalsPath.isEmpty() && QFileInfo(m_spleeterVocalsPath).isFile()) {
        return m_spleeterVocalsPath;
    }
    AppController *app = AppController::instance();
    if (app && app->models()) {
        for (const QString &modelId : {QStringLiteral("k2-fsa/sherpa-onnx-spleeter-2stems-fp16"),
                                      QStringLiteral("k2-fsa/sherpa-onnx-source-separation")}) {
            const QString path = app->models()->filePath(modelId, QStringLiteral("vocals.fp16.onnx"));
            if (!path.isEmpty() && QFileInfo(path).isFile()) return path;
        }
    }
    return QString();
}

QString VoiceIsolatorController::spleeterAccompanimentPath() const
{
    if (m_hasActiveConfig) {
        return m_activeConfig.modelFilesByRole.value(QStringLiteral("accompaniment-model"));
    }
    if (m_configurationOverrideActive) {
        return QFileInfo(m_spleeterAccompanimentPath).isFile() ? m_spleeterAccompanimentPath : QString();
    }
    if (!m_spleeterAccompanimentPath.isEmpty() && QFileInfo(m_spleeterAccompanimentPath).isFile()) {
        return m_spleeterAccompanimentPath;
    }
    AppController *app = AppController::instance();
    if (app && app->models()) {
        for (const QString &modelId : {QStringLiteral("k2-fsa/sherpa-onnx-spleeter-2stems-fp16"),
                                      QStringLiteral("k2-fsa/sherpa-onnx-source-separation")}) {
            const QString path = app->models()->filePath(modelId, QStringLiteral("accompaniment.fp16.onnx"));
            if (!path.isEmpty() && QFileInfo(path).isFile()) return path;
        }
    }
    return QString();
}

bool VoiceIsolatorController::ready() const
{
    if (m_hasActiveConfig) {
        const bool runtimeReady = !m_activeConfig.runtimePath.isEmpty();
        const bool uvrReady = m_activeConfig.modelFilesByRole.contains(QStringLiteral("model"));
        const bool spleeterReady = m_activeConfig.modelFilesByRole.contains(QStringLiteral("vocals-model")) && 
                                   m_activeConfig.modelFilesByRole.contains(QStringLiteral("accompaniment-model"));
        return runtimeReady && (uvrReady || spleeterReady);
    }
    const QString rt = runtimePath();
    const bool runtimeReady = !rt.isEmpty();
    const bool uvrReady = !modelPath().isEmpty();
    const bool spleeterReady = !spleeterVocalsPath().isEmpty() && !spleeterAccompanimentPath().isEmpty();
    return runtimeReady && (uvrReady || spleeterReady);
}

void VoiceIsolatorController::setSourcePath(const QString &path) { if (m_sourcePath == path) return; m_sourcePath = path; emit stateChanged(); }
void VoiceIsolatorController::setRuntimePath(const QString &path) { if (m_runtimePath == path && m_configurationOverrideActive) return; m_configurationOverrideActive = true; m_runtimePath = path; emit stateChanged(); }
void VoiceIsolatorController::setModelPath(const QString &path) { if (m_modelPath == path && m_configurationOverrideActive) return; m_configurationOverrideActive = true; m_modelPath = path; emit stateChanged(); }
void VoiceIsolatorController::setThreadCount(int value) { value = qBound(1, value, 64); if (m_threadCount == value) return; m_threadCount = value; emit stateChanged(); }

void VoiceIsolatorController::applyModelConfiguration(const QString &runtimePath, const QVariantMap &resolvedPaths)
{
    m_configurationOverrideActive = true;
    m_hasActiveConfig = false;
    m_runtimePath = runtimePath;
    m_modelPath = resolvedPaths.value(QStringLiteral("model")).toString();
    m_spleeterVocalsPath = resolvedPaths.value(QStringLiteral("vocals-model")).toString();
    m_spleeterAccompanimentPath = resolvedPaths.value(QStringLiteral("accompaniment-model")).toString();
    emit stateChanged();
}

void VoiceIsolatorController::applySeparationConfiguration(const SeparationConfiguration &config)
{
    m_activeConfig = config;
    m_hasActiveConfig = true;
    m_configurationOverrideActive = false;
    emit stateChanged();
}

void VoiceIsolatorController::clearModelConfiguration()
{
    m_configurationOverrideActive = true;
    m_hasActiveConfig = false;
    m_activeConfig = {};
    m_runtimePath.clear();
    m_modelPath.clear();
    m_spleeterVocalsPath.clear();
    m_spleeterAccompanimentPath.clear();
    emit stateChanged();
}

void VoiceIsolatorController::isolate(bool fast)
{
    if (m_sourcePath.isEmpty()) {
        m_lastError = QStringLiteral("Choose an audio or video file first.");
        emit stateChanged();
        return;
    }

    SeparationConfiguration config;
    if (m_hasActiveConfig) {
        config = m_activeConfig;
    } else {
        const QString rt = runtimePath();
        const QString uvr = modelPath();
        const QString spVoc = spleeterVocalsPath();
        const QString spAcc = spleeterAccompanimentPath();

        if (rt.isEmpty()) { m_lastError = QStringLiteral("Select a sherpa-onnx runtime first."); emit stateChanged(); return; }
        if (fast && (spVoc.isEmpty() || spAcc.isEmpty())) { m_lastError = QStringLiteral("Spleeter requires both vocals and accompaniment model files."); emit stateChanged(); return; }
        if (!fast && uvr.isEmpty()) { m_lastError = QStringLiteral("UVR requires UVR-MDX-NET-Voc_FT.onnx."); emit stateChanged(); return; }

        config.backendId = QStringLiteral("sherpa-onnx");
        config.pipelineProfile = fast ? QStringLiteral("spleeter-2stems") : QStringLiteral("uvr-2stems");
        config.runtimeId = QStringLiteral("sherpa-onnx-manual");
        config.runtimeVersion = QStringLiteral("manual");
        config.runtimePath = rt;
        config.familyId = QStringLiteral("manual");
        config.configurationSignature = QStringLiteral("manual");

        if (fast) {
            config.modelFilesByRole.insert(QStringLiteral("vocals-model"), spVoc);
            config.modelFilesByRole.insert(QStringLiteral("accompaniment-model"), spAcc);
        } else {
            config.modelFilesByRole.insert(QStringLiteral("model"), uvr);
        }
    }

    m_processing = true;
    m_progress = 0;
    m_lastError.clear();
    m_warning.clear();
    emit stateChanged();

    m_tempDir.reset();
    m_tempDir = std::make_unique<QTemporaryDir>(
        QDir(QDir::tempPath()).filePath(kIsolationTempPrefix + QStringLiteral("XXXXXX")));
    m_recentResults.clear();
    if (!m_tempDir->isValid()) {
        m_processing = false;
        m_lastError = QStringLiteral("Failed to create temporary directory for isolation.");
        emit stateChanged();
        return;
    }

    SeparationRequest req;
    req.sourcePath = m_sourcePath;
    req.outputRoot = m_tempDir->path();
    req.configuration = config;
    req.numThreads = m_threadCount;

    QString err;
    if (!m_service->isolate(req, &err)) {
        m_processing = false;
        m_lastError = err;
        emit stateChanged();
    }
}

void VoiceIsolatorController::cancel() { m_service->cancel(); m_warning = QStringLiteral("Cancellation requested; the current inference will be discarded when it returns."); emit stateChanged(); }
void VoiceIsolatorController::clearResult()
{
    m_vocalsPath.clear();
    m_backgroundPath.clear();
    m_vocalsSamples.clear();
    m_backgroundSamples.clear();
    m_lastError.clear();
    m_warning.clear();
    m_tempDir.reset();
    m_recentResults.clear();
    emit vocalsSamplesChanged();
    emit backgroundSamplesChanged();
    emit stateChanged();
}

bool VoiceIsolatorController::exportStem(const QString &sourcePath, const QString &destinationPath)
{
    if (sourcePath.isEmpty() || destinationPath.isEmpty() || !QFileInfo(sourcePath).isFile()) return false;
    const QString target = destinationPath.startsWith(QStringLiteral("file:///")) ? QUrl(destinationPath).toLocalFile() : destinationPath;
    const QFileInfo info(target);
    QDir().mkpath(info.absolutePath());
    const QString staging = target + QStringLiteral(".staging");
    QFile::remove(staging);
    if (!QFile::copy(sourcePath, staging)) return false;
    QFile::remove(target);
    return QFile::rename(staging, target);
}

void VoiceIsolatorController::openRecent(const QString &vocalsPath, const QString &backgroundPath)
{
    m_vocalsPath = vocalsPath;
    m_backgroundPath = backgroundPath;
    m_lastError.clear();
    loadVocalsSamples(m_vocalsPath);
    loadBackgroundSamples(m_backgroundPath);
    emit stateChanged();
}

void VoiceIsolatorController::loadVocalsSamples(const QString &path)
{
    m_vocalsSamples.clear();
    emit vocalsSamplesChanged();
    if (path.isEmpty()) return;

    QString cleanPath = path;
    if (cleanPath.startsWith(QStringLiteral("file:///"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    cleanPath = QDir::toNativeSeparators(cleanPath);

    QPointer<VoiceIsolatorController> weakThis(this);
    QThreadPool::globalInstance()->start([weakThis, cleanPath]() {
        WavIO::WavData data = AudioFileDecoder::decode(cleanPath);
        QVariantList list;
        if (!data.samples.isEmpty()) {
            int step = std::max<int>(1, data.samples.size() / 1000);
            list.reserve(data.samples.size() / step + 1);
            for (int i = 0; i < data.samples.size(); i += step) {
                list.append(data.samples[i]);
            }
        }

        QCoreApplication* app = QCoreApplication::instance();
        if (app) {
            QMetaObject::invokeMethod(app, [weakThis, list]() {
                if (weakThis) {
                    weakThis->m_vocalsSamples = list;
                    emit weakThis->vocalsSamplesChanged();
                }
            });
        }
    });
}

void VoiceIsolatorController::loadBackgroundSamples(const QString &path)
{
    m_backgroundSamples.clear();
    emit backgroundSamplesChanged();
    if (path.isEmpty()) return;

    QString cleanPath = path;
    if (cleanPath.startsWith(QStringLiteral("file:///"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    cleanPath = QDir::toNativeSeparators(cleanPath);

    QPointer<VoiceIsolatorController> weakThis(this);
    QThreadPool::globalInstance()->start([weakThis, cleanPath]() {
        WavIO::WavData data = AudioFileDecoder::decode(cleanPath);
        QVariantList list;
        if (!data.samples.isEmpty()) {
            int step = std::max<int>(1, data.samples.size() / 1000);
            list.reserve(data.samples.size() / step + 1);
            for (int i = 0; i < data.samples.size(); i += step) {
                list.append(data.samples[i]);
            }
        }

        QCoreApplication* app = QCoreApplication::instance();
        if (app) {
            QMetaObject::invokeMethod(app, [weakThis, list]() {
                if (weakThis) {
                    weakThis->m_backgroundSamples = list;
                    emit weakThis->backgroundSamplesChanged();
                }
            });
        }
    });
}

void VoiceIsolatorController::addRecent(const QVariantMap &result)
{
    QVariantMap entry{{QStringLiteral("vocalsPath"), result.value(QStringLiteral("vocalsPath"))}, {QStringLiteral("backgroundPath"), result.value(QStringLiteral("backgroundPath"))}, {QStringLiteral("sourceHash"), result.value(QStringLiteral("sourceHash"))}};
    // There is one session-owned temp directory. Keeping older entries would
    // leave invalid paths after the next run cleans that directory.
    m_recentResults = QVariantList{entry};
}

} // namespace LAStudio
