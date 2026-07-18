#include "VoiceDesignPresetService.h"
#include "core/PathUtils.h"
#include "core/Logger.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>

namespace LAStudio {

VoiceDesignPresetService::VoiceDesignPresetService(QObject *parent)
    : QObject(parent)
{
}

QString VoiceDesignPresetService::presetsFilePath() const
{
    return PathUtils::dataDir() + QStringLiteral("/presets/voice_design_presets.json");
}

QVariantList VoiceDesignPresetService::loadAllPresets() const
{
    QVariantList list;
    QFile file(presetsFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return list;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
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

bool VoiceDesignPresetService::saveAllPresets(const QVariantList &presets)
{
    QString path = presetsFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error("VoiceDesignPresetService", "Failed to write presets file: " + path);
        emit errorOccurred(QStringLiteral("Failed to save presets locally."));
        return false;
    }

    QJsonArray arr;
    for (const QVariant &item : presets) {
        arr.append(QJsonObject::fromVariantMap(item.toMap()));
    }

    QJsonDocument doc(arr);
    file.write(doc.toJson());
    file.close();
    return true;
}

QVariantList VoiceDesignPresetService::presetsForFamily(const QString &familyId)
{
    QVariantList all = loadAllPresets();
    QVariantList filtered;
    for (const QVariant &val : all) {
        QVariantMap preset = val.toMap();
        if (preset.value(QStringLiteral("familyId")).toString() == familyId) {
            filtered.append(preset);
        }
    }
    return filtered;
}

bool VoiceDesignPresetService::addPreset(const QString &familyId, const QString &name, const QString &description)
{
    if (familyId.isEmpty() || name.trimmed().isEmpty() || description.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Preset details cannot be empty."));
        return false;
    }

    QVariantList all = loadAllPresets();
    
    QVariantMap preset;
    QString id = QString::number(QDateTime::currentMSecsSinceEpoch());
    QString nowStr = QDateTime::currentDateTime().toString(Qt::ISODate);

    preset.insert(QStringLiteral("id"), id);
    preset.insert(QStringLiteral("familyId"), familyId);
    preset.insert(QStringLiteral("name"), name.trimmed());
    preset.insert(QStringLiteral("description"), description.trimmed());
    preset.insert(QStringLiteral("createdAt"), nowStr);
    preset.insert(QStringLiteral("updatedAt"), nowStr);

    all.prepend(preset);

    if (saveAllPresets(all)) {
        emit presetsChanged(familyId);
        return true;
    }
    return false;
}

bool VoiceDesignPresetService::updatePreset(const QString &id, const QString &name, const QString &description)
{
    if (id.isEmpty() || name.trimmed().isEmpty() || description.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Preset details cannot be empty."));
        return false;
    }

    QVariantList all = loadAllPresets();
    bool found = false;
    QString familyId;

    for (int i = 0; i < all.size(); ++i) {
        QVariantMap preset = all[i].toMap();
        if (preset.value(QStringLiteral("id")).toString() == id) {
            familyId = preset.value(QStringLiteral("familyId")).toString();
            preset.insert(QStringLiteral("name"), name.trimmed());
            preset.insert(QStringLiteral("description"), description.trimmed());
            preset.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
            all[i] = preset;
            found = true;
            break;
        }
    }

    if (!found) {
        emit errorOccurred(QStringLiteral("Preset not found."));
        return false;
    }

    if (saveAllPresets(all)) {
        emit presetsChanged(familyId);
        return true;
    }
    return false;
}

bool VoiceDesignPresetService::deletePreset(const QString &id)
{
    if (id.isEmpty()) {
        return false;
    }

    QVariantList all = loadAllPresets();
    QVariantList remaining;
    QString familyId;
    bool found = false;

    for (const QVariant &val : all) {
        QVariantMap preset = val.toMap();
        if (preset.value(QStringLiteral("id")).toString() == id) {
            familyId = preset.value(QStringLiteral("familyId")).toString();
            found = true;
        } else {
            remaining.append(preset);
        }
    }

    if (!found) {
        return false;
    }

    if (saveAllPresets(remaining)) {
        emit presetsChanged(familyId);
        return true;
    }
    return false;
}

} // namespace LAStudio
