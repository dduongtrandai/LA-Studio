#include "Settings.h"
#include "PathUtils.h"
#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace LAStudio {

namespace {

QString settingsFilePath()
{
    return PathUtils::dataDir() + QStringLiteral("/settings.json");
}

QString settingsIniPath()
{
    return PathUtils::dataDir() + QStringLiteral("/settings.ini");
}

bool hasModelFiles(const QString &path)
{
    QDir root(path);
    if (!root.exists()) return false;

    QFile registry(root.absoluteFilePath(QStringLiteral("registry.json")));
    if (registry.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(registry.readAll());
        if (doc.isArray() && !doc.array().isEmpty()) {
            return true;
        }
    }

    const QStringList subDirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subDirName : subDirs) {
        QDir subDir(root.absoluteFilePath(subDirName));
        const QStringList modelFiles = subDir.entryList(
            {QStringLiteral("*.gguf"), QStringLiteral("*.bin"), QStringLiteral("*.onnx")},
            QDir::Files);
        if (!modelFiles.isEmpty()) {
            return true;
        }
    }

    return false;
}

QString readStableModelsPath()
{
    QFile file(settingsFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }

    const QJsonObject storage = doc.object().value(QStringLiteral("storage")).toObject();
    const QString path = storage.value(QStringLiteral("modelsPath")).toString();
    return path.isEmpty() ? QString() : QDir(path).absolutePath();
}

void writeStableModelsPath(const QString &path)
{
    QDir().mkpath(PathUtils::dataDir());

    QJsonObject root;
    QFile existing(settingsFilePath());
    if (existing.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(existing.readAll());
        if (doc.isObject()) {
            root = doc.object();
        }
    }

    QJsonObject storage = root.value(QStringLiteral("storage")).toObject();
    storage[QStringLiteral("modelsPath")] = QDir(path).absolutePath();
    root[QStringLiteral("storage")] = storage;

    QFile file(settingsFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

QString discoverExistingModelsPath(const QString &qSettingsPath)
{
    QStringList candidates;
    const QString stablePath = readStableModelsPath();
    if (!stablePath.isEmpty()) candidates.append(stablePath);
    if (!qSettingsPath.isEmpty()) candidates.append(QDir(qSettingsPath).absolutePath());
    candidates.append(QDir(PathUtils::modelsDir()).absolutePath());

#ifdef Q_OS_WIN
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo &drive : drives) {
        candidates.append(QDir(drive.absoluteFilePath()).absoluteFilePath(QStringLiteral("models")));
    }
#endif

    QSet<QString> seen;
    for (const QString &candidate : candidates) {
        const QString normalized = QDir(candidate).absolutePath();
        if (seen.contains(normalized)) continue;
        seen.insert(normalized);
        if (hasModelFiles(normalized)) {
            return normalized;
        }
    }

    return qSettingsPath.isEmpty() ? QDir(PathUtils::modelsDir()).absolutePath() : QDir(qSettingsPath).absolutePath();
}

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings(settingsIniPath(), QSettings::IniFormat)
{
    // Initialize cached values from QSettings
    m_device = m_settings.value(QStringLiteral("engine/device"), QStringLiteral("cpu")).toString();
    m_threads = m_settings.value(QStringLiteral("engine/threads"), 4).toInt();
    m_language = m_settings.value(QStringLiteral("engine/language"), QStringLiteral("en")).toString();
    m_uiLanguage = m_settings.value(QStringLiteral("ui/language"), QStringLiteral("en")).toString();
    const QString qSettingsModelsPath = m_settings.value(QStringLiteral("storage/modelsPath"), PathUtils::modelsDir()).toString();
    m_modelsPath = discoverExistingModelsPath(qSettingsModelsPath);
    m_settings.setValue(QStringLiteral("storage/modelsPath"), m_modelsPath);
    m_settings.sync();
    writeStableModelsPath(m_modelsPath);
    Logger::info(QStringLiteral("Settings"), QStringLiteral("Models path: %1").arg(m_modelsPath));
    m_selectedRuntime = m_settings.value(QStringLiteral("engine/selectedRuntime"), QString()).toString();
    m_selectedTtsRuntime = m_settings.value(QStringLiteral("engine/selectedTtsRuntime"), QString()).toString();
    m_selectedTtsRuntimeVersion = m_settings.value(QStringLiteral("engine/selectedTtsRuntimeVersion"), QString()).toString();
    m_selectedTtsFamily = m_settings.value(QStringLiteral("tts/selectedFamily"), QString()).toString();
    m_selectedVoiceCloneFamily = m_settings.value(QStringLiteral("voiceCloning/selectedFamily"), QString()).toString();
    m_selectedSttRuntime = m_settings.value(QStringLiteral("stt/selectedRuntime"), QString()).toString();
    m_selectedSttRuntimeVersion = m_settings.value(QStringLiteral("engine/selectedSttRuntimeVersion"), QString()).toString();
    m_selectedSttFamily = m_settings.value(QStringLiteral("stt/selectedFamily"), QString()).toString();
    m_selectedSttModelPath = m_settings.value(QStringLiteral("stt/selectedModelPath"), QString()).toString();
    m_selectedSttModelFile = m_settings.value(QStringLiteral("stt/selectedModelFile"), QString()).toString();
    m_sttLanguage = m_settings.value(QStringLiteral("stt/language"), QStringLiteral("auto")).toString();
    m_sttThreads = m_settings.value(QStringLiteral("stt/threads"), 0).toInt();
    m_sttTranslate = m_settings.value(QStringLiteral("stt/translate"), false).toBool();
    m_offloadKvCache = m_settings.value(QStringLiteral("hardware/offloadKvCache"), true).toBool();
    m_guardrailMode = m_settings.value(QStringLiteral("hardware/guardrailMode"), 3).toInt(); // Default to Strict (index 3)
    m_apiServerEnabled = m_settings.value(QStringLiteral("api/serverEnabled"), false).toBool();
    m_apiServerAllowLan = m_settings.value(QStringLiteral("api/serverAllowLan"), false).toBool();
    m_apiServerPort = m_settings.value(QStringLiteral("api/serverPort"), 3900).toInt();
    m_apiServerApiKey = m_settings.value(QStringLiteral("api/serverApiKey"), QString()).toString();
}


QString Settings::device() const
{
    return m_device;
}

void Settings::setDevice(const QString &v)
{
    if (m_device != v) {
        m_device = v;
        m_settings.setValue(QStringLiteral("engine/device"), v);
        m_settings.sync();
        emit deviceChanged();
    }
}

int Settings::threads() const
{
    return m_threads;
}

void Settings::setThreads(int v)
{
    if (m_threads != v) {
        m_threads = v;
        m_settings.setValue(QStringLiteral("engine/threads"), v);
        m_settings.sync();
        emit threadsChanged();
    }
}

QString Settings::language() const
{
    return m_language;
}

void Settings::setLanguage(const QString &v)
{
    if (m_language != v) {
        m_language = v;
        m_settings.setValue(QStringLiteral("engine/language"), v);
        m_settings.sync();
        emit languageChanged();
    }
}

QString Settings::uiLanguage() const
{
    return m_uiLanguage;
}

void Settings::setUiLanguage(const QString &v)
{
    if (m_uiLanguage != v) {
        m_uiLanguage = v;
        m_settings.setValue(QStringLiteral("ui/language"), v);
        m_settings.sync();
        emit uiLanguageChanged();
    }
}

QString Settings::modelsPath() const
{
    return m_modelsPath;
}

void Settings::setModelsPath(const QString &v)
{
    const QString normalized = QDir(v).absolutePath();
    if (m_modelsPath != normalized) {
        m_modelsPath = normalized;
        m_settings.setValue(QStringLiteral("storage/modelsPath"), normalized);
        m_settings.sync();
        writeStableModelsPath(normalized);
        emit modelsPathChanged();
    }
}

QString Settings::selectedRuntime() const
{
    return m_selectedRuntime;
}

void Settings::setSelectedRuntime(const QString &v)
{
    if (m_selectedRuntime != v) {
        m_selectedRuntime = v;
        m_settings.setValue(QStringLiteral("engine/selectedRuntime"), v);
        m_settings.sync();
        emit selectedRuntimeChanged();
    }
}

QString Settings::selectedTtsRuntime() const
{
    return m_selectedTtsRuntime;
}

void Settings::setSelectedTtsRuntime(const QString &v)
{
    if (m_selectedTtsRuntime != v) {
        m_selectedTtsRuntime = v;
        m_settings.setValue(QStringLiteral("engine/selectedTtsRuntime"), v);
        m_settings.sync();
        emit selectedTtsRuntimeChanged();
    }
}

QString Settings::selectedTtsRuntimeVersion() const
{
    return m_selectedTtsRuntimeVersion;
}

void Settings::setSelectedTtsRuntimeVersion(const QString &v)
{
    if (m_selectedTtsRuntimeVersion != v) {
        m_selectedTtsRuntimeVersion = v;
        m_settings.setValue(QStringLiteral("engine/selectedTtsRuntimeVersion"), v);
        m_settings.sync();
        emit selectedTtsRuntimeVersionChanged();
    }
}

QString Settings::selectedTtsFamily() const
{
    return m_selectedTtsFamily;
}

void Settings::setSelectedTtsFamily(const QString &v)
{
    if (m_selectedTtsFamily != v) {
        m_selectedTtsFamily = v;
        m_settings.setValue(QStringLiteral("tts/selectedFamily"), v);
        m_settings.sync();
        emit selectedTtsFamilyChanged();
    }
}

QString Settings::selectedVoiceCloneFamily() const
{
    return m_selectedVoiceCloneFamily;
}

void Settings::setSelectedVoiceCloneFamily(const QString &v)
{
    if (m_selectedVoiceCloneFamily != v) {
        m_selectedVoiceCloneFamily = v;
        m_settings.setValue(QStringLiteral("voiceCloning/selectedFamily"), v);
        m_settings.sync();
        emit selectedVoiceCloneFamilyChanged();
    }
}

QString Settings::selectedSttRuntime() const
{
    return m_selectedSttRuntime;
}

void Settings::setSelectedSttRuntime(const QString &v)
{
    if (m_selectedSttRuntime != v) {
        m_selectedSttRuntime = v;
        m_settings.setValue(QStringLiteral("stt/selectedRuntime"), v);
        m_settings.sync();
        emit selectedSttRuntimeChanged();
    }
}

QString Settings::selectedSttRuntimeVersion() const
{
    return m_selectedSttRuntimeVersion;
}

void Settings::setSelectedSttRuntimeVersion(const QString &v)
{
    if (m_selectedSttRuntimeVersion != v) {
        m_selectedSttRuntimeVersion = v;
        m_settings.setValue(QStringLiteral("engine/selectedSttRuntimeVersion"), v);
        m_settings.sync();
        emit selectedSttRuntimeVersionChanged();
    }
}

QString Settings::selectedSttFamily() const
{
    return m_selectedSttFamily;
}

void Settings::setSelectedSttFamily(const QString &v)
{
    if (m_selectedSttFamily != v) {
        m_selectedSttFamily = v;
        m_settings.setValue(QStringLiteral("stt/selectedFamily"), v);
        m_settings.sync();
        emit selectedSttFamilyChanged();
    }
}

QString Settings::selectedSttModelPath() const
{
    return m_selectedSttModelPath;
}

void Settings::setSelectedSttModelPath(const QString &v)
{
    if (m_selectedSttModelPath != v) {
        m_selectedSttModelPath = v;
        m_settings.setValue(QStringLiteral("stt/selectedModelPath"), v);
        m_settings.sync();
        emit selectedSttModelPathChanged();
    }
}

QString Settings::selectedSttModelFile() const
{
    return m_selectedSttModelFile;
}

void Settings::setSelectedSttModelFile(const QString &v)
{
    if (m_selectedSttModelFile != v) {
        m_selectedSttModelFile = v;
        m_settings.setValue(QStringLiteral("stt/selectedModelFile"), v);
        m_settings.sync();
        emit selectedSttModelFileChanged();
    }
}

QString Settings::sttLanguage() const
{
    return m_sttLanguage;
}

void Settings::setSttLanguage(const QString &v)
{
    if (m_sttLanguage != v) {
        m_sttLanguage = v;
        m_settings.setValue(QStringLiteral("stt/language"), v);
        m_settings.sync();
        emit sttLanguageChanged();
    }
}

int Settings::sttThreads() const
{
    return m_sttThreads;
}

void Settings::setSttThreads(int v)
{
    v = qBound(0, v, 64);
    if (m_sttThreads != v) {
        m_sttThreads = v;
        m_settings.setValue(QStringLiteral("stt/threads"), v);
        m_settings.sync();
        emit sttThreadsChanged();
    }
}

bool Settings::sttTranslate() const
{
    return m_sttTranslate;
}

void Settings::setSttTranslate(bool v)
{
    if (m_sttTranslate != v) {
        m_sttTranslate = v;
        m_settings.setValue(QStringLiteral("stt/translate"), v);
        m_settings.sync();
        emit sttTranslateChanged();
    }
}

bool Settings::offloadKvCache() const
{
    return m_offloadKvCache;
}

void Settings::setOffloadKvCache(bool v)
{
    if (m_offloadKvCache != v) {
        m_offloadKvCache = v;
        m_settings.setValue(QStringLiteral("hardware/offloadKvCache"), v);
        m_settings.sync();
        emit offloadKvCacheChanged();
    }
}

int Settings::guardrailMode() const
{
    return m_guardrailMode;
}

void Settings::setGuardrailMode(int v)
{
    if (m_guardrailMode != v) {
        m_guardrailMode = v;
        m_settings.setValue(QStringLiteral("hardware/guardrailMode"), v);
        m_settings.sync();
        emit guardrailModeChanged();
    }
}

bool Settings::apiServerEnabled() const
{
    return m_apiServerEnabled;
}

void Settings::setApiServerEnabled(bool v)
{
    if (m_apiServerEnabled != v) {
        m_apiServerEnabled = v;
        m_settings.setValue(QStringLiteral("api/serverEnabled"), v);
        m_settings.sync();
        emit apiServerEnabledChanged();
    }
}

bool Settings::apiServerAllowLan() const
{
    return m_apiServerAllowLan;
}

void Settings::setApiServerAllowLan(bool v)
{
    if (m_apiServerAllowLan != v) {
        m_apiServerAllowLan = v;
        m_settings.setValue(QStringLiteral("api/serverAllowLan"), v);
        m_settings.sync();
        emit apiServerAllowLanChanged();
    }
}

int Settings::apiServerPort() const
{
    return m_apiServerPort;
}

void Settings::setApiServerPort(int v)
{
    v = qBound(1, v, 65535);
    if (m_apiServerPort != v) {
        m_apiServerPort = v;
        m_settings.setValue(QStringLiteral("api/serverPort"), v);
        m_settings.sync();
        emit apiServerPortChanged();
    }
}

QString Settings::apiServerApiKey() const
{
    return m_apiServerApiKey;
}

void Settings::setApiServerApiKey(const QString &v)
{
    const QString normalized = v.trimmed();
    if (m_apiServerApiKey != normalized) {
        m_apiServerApiKey = normalized;
        m_settings.setValue(QStringLiteral("api/serverApiKey"), normalized);
        m_settings.sync();
        emit apiServerApiKeyChanged();
    }
}

} // namespace LAStudio

