#include "VoiceClonePresetService.h"
#include "core/Logger.h"
#include "core/PathUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace LAStudio {

VoiceClonePresetService::VoiceClonePresetService(QObject *parent)
    : QObject(parent)
{
}

QString VoiceClonePresetService::presetsFilePath() const
{
    return PathUtils::dataDir() + QStringLiteral("/presets/voice_clone_presets.json");
}

QString VoiceClonePresetService::audioStorageDir() const
{
    return PathUtils::dataDir() + QStringLiteral("/presets/voice_clone_refs");
}

QVariantList VoiceClonePresetService::loadAllPresets() const
{
    QVariantList list;
    QFile file(presetsFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return list;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return list;
    }

    QJsonArray arr = doc.array();
    list.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        if (val.isObject()) {
            list.append(val.toObject().toVariantMap());
        }
    }
    return list;
}

bool VoiceClonePresetService::saveAllPresets(const QVariantList &presets)
{
    const QString path = presetsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("VoiceClonePresetService", "Failed to write presets file: " + path);
        emit errorOccurred(QStringLiteral("Failed to save voice clone presets locally."));
        return false;
    }

    QJsonArray arr;
    for (const QVariant &item : presets) {
        arr.append(QJsonObject::fromVariantMap(item.toMap()));
    }

    file.write(QJsonDocument(arr).toJson());
    file.close();
    return true;
}

QString VoiceClonePresetService::persistReferenceAudio(const QString &id, const QString &audioPath)
{
    const QString sourcePath = PathUtils::urlToLocalPath(audioPath);
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        emit errorOccurred(QStringLiteral("Reference audio file was not found."));
        return {};
    }

    QDir().mkpath(audioStorageDir());

    QString suffix = QFileInfo(sourcePath).suffix().toLower();
    if (suffix.isEmpty()) {
        suffix = QStringLiteral("wav");
    }

    const QString destPath = audioStorageDir() + QStringLiteral("/") + id + QStringLiteral(".") + suffix;
    if (QFileInfo(sourcePath).absoluteFilePath() == QFileInfo(destPath).absoluteFilePath()) {
        return destPath;
    }

    QFile::remove(destPath);
    if (!QFile::copy(sourcePath, destPath)) {
        Logger::error("VoiceClonePresetService", "Failed to copy reference audio to: " + destPath);
        emit errorOccurred(QStringLiteral("Failed to save reference audio locally."));
        return {};
    }
    return destPath;
}

void VoiceClonePresetService::removeStoredReferenceAudio(const QString &audioPath)
{
    const QString localPath = PathUtils::urlToLocalPath(audioPath);
    if (localPath.isEmpty()) {
        return;
    }

    const QString storagePath = QFileInfo(audioStorageDir()).absoluteFilePath();
    const QFileInfo info(localPath);
    if (info.exists() && info.absoluteFilePath().startsWith(storagePath)) {
        QFile::remove(info.absoluteFilePath());
    }
}

QVariantList VoiceClonePresetService::presetsForFamily(const QString &familyId)
{
    QVariantList filtered;
    for (const QVariant &val : loadAllPresets()) {
        QVariantMap preset = val.toMap();
        if (preset.value(QStringLiteral("familyId")).toString() == familyId) {
            filtered.append(preset);
        }
    }
    return filtered;
}

bool VoiceClonePresetService::addPreset(const QString &familyId,
                                        const QString &name,
                                        const QString &audioPath,
                                        const QString &referenceText)
{
    if (familyId.isEmpty() || name.trimmed().isEmpty() || audioPath.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Voice name and reference audio are required."));
        return false;
    }

    QVariantList all = loadAllPresets();
    const QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString storedAudioPath = persistReferenceAudio(id, audioPath);
    if (storedAudioPath.isEmpty()) {
        return false;
    }

    const QString nowStr = QDateTime::currentDateTime().toString(Qt::ISODate);
    QVariantMap preset;
    preset.insert(QStringLiteral("id"), id);
    preset.insert(QStringLiteral("familyId"), familyId);
    preset.insert(QStringLiteral("name"), name.trimmed());
    preset.insert(QStringLiteral("audioPath"), storedAudioPath);
    preset.insert(QStringLiteral("referenceText"), referenceText.trimmed());
    preset.insert(QStringLiteral("originalAudioName"), QFileInfo(PathUtils::urlToLocalPath(audioPath)).fileName());
    preset.insert(QStringLiteral("createdAt"), nowStr);
    preset.insert(QStringLiteral("updatedAt"), nowStr);

    all.prepend(preset);

    if (saveAllPresets(all)) {
        emit presetsChanged(familyId);
        return true;
    }

    removeStoredReferenceAudio(storedAudioPath);
    return false;
}

bool VoiceClonePresetService::updatePreset(const QString &id,
                                           const QString &name,
                                           const QString &audioPath,
                                           const QString &referenceText)
{
    if (id.isEmpty() || name.trimmed().isEmpty() || audioPath.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Voice name and reference audio are required."));
        return false;
    }

    QVariantList all = loadAllPresets();
    QString familyId;
    QString oldAudioPath;
    bool found = false;

    for (int i = 0; i < all.size(); ++i) {
        QVariantMap preset = all[i].toMap();
        if (preset.value(QStringLiteral("id")).toString() != id) {
            continue;
        }

        familyId = preset.value(QStringLiteral("familyId")).toString();
        oldAudioPath = preset.value(QStringLiteral("audioPath")).toString();
        QString storedAudioPath = oldAudioPath;
        if (QFileInfo(PathUtils::urlToLocalPath(audioPath)).absoluteFilePath()
            != QFileInfo(PathUtils::urlToLocalPath(oldAudioPath)).absoluteFilePath()) {
            storedAudioPath = persistReferenceAudio(id, audioPath);
            if (storedAudioPath.isEmpty()) {
                return false;
            }
        }

        preset.insert(QStringLiteral("name"), name.trimmed());
        preset.insert(QStringLiteral("audioPath"), storedAudioPath);
        preset.insert(QStringLiteral("referenceText"), referenceText.trimmed());
        preset.insert(QStringLiteral("originalAudioName"), QFileInfo(PathUtils::urlToLocalPath(audioPath)).fileName());
        preset.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
        all[i] = preset;
        found = true;
        break;
    }

    if (!found) {
        emit errorOccurred(QStringLiteral("Voice preset not found."));
        return false;
    }

    if (saveAllPresets(all)) {
        if (!oldAudioPath.isEmpty() && oldAudioPath != audioPath) {
            removeStoredReferenceAudio(oldAudioPath);
        }
        emit presetsChanged(familyId);
        return true;
    }
    return false;
}

bool VoiceClonePresetService::deletePreset(const QString &id)
{
    if (id.isEmpty()) {
        return false;
    }

    QVariantList all = loadAllPresets();
    QVariantList remaining;
    QString familyId;
    QString audioPath;
    bool found = false;

    for (const QVariant &val : all) {
        QVariantMap preset = val.toMap();
        if (preset.value(QStringLiteral("id")).toString() == id) {
            familyId = preset.value(QStringLiteral("familyId")).toString();
            audioPath = preset.value(QStringLiteral("audioPath")).toString();
            found = true;
        } else {
            remaining.append(preset);
        }
    }

    if (!found) {
        return false;
    }

    if (saveAllPresets(remaining)) {
        removeStoredReferenceAudio(audioPath);
        emit presetsChanged(familyId);
        return true;
    }
    return false;
}

} // namespace LAStudio
