#include "dubbing/DubbingTimingProfile.h"

#include "core/PathUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QCryptographicHash>

namespace LAStudio {

QVariantMap DubbingTimingProfile::toVariantMap() const
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("voiceSignature"), voiceSignature},
            {QStringLiteral("language"), language}, {QStringLiteral("phonemesPerSecond"), phonemesPerSecond},
            {QStringLiteral("sampleCount"), sampleCount}, {QStringLiteral("confidence"), confidence},
            {QStringLiteral("normalizerVersion"), normalizerVersion}};
}

DubbingTimingProfile DubbingTimingProfile::fromVariantMap(const QVariantMap &map)
{
    DubbingTimingProfile result;
    result.id = map.value(QStringLiteral("id")).toString();
    result.voiceSignature = map.value(QStringLiteral("voiceSignature")).toString();
    result.language = map.value(QStringLiteral("language"), result.language).toString();
    result.phonemesPerSecond = map.value(QStringLiteral("phonemesPerSecond"), result.phonemesPerSecond).toDouble();
    result.sampleCount = map.value(QStringLiteral("sampleCount")).toInt();
    result.confidence = map.value(QStringLiteral("confidence")).toDouble();
    result.normalizerVersion = map.value(QStringLiteral("normalizerVersion"), result.normalizerVersion).toString();
    return result;
}

QString DubbingTimingProfileStore::profileId(const QString &voiceSignature, const QString &language,
                                             const QString &normalizerVersion)
{
    const QByteArray value = (voiceSignature + QLatin1Char('|') + language + QLatin1Char('|') + normalizerVersion).toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(value, QCryptographicHash::Sha256).toHex());
}

static QString profilePath(const QString &id)
{
    return QDir(PathUtils::dataDir() + QStringLiteral("/dubbing/timing-profiles")).filePath(id + QStringLiteral(".json"));
}

bool DubbingTimingProfileStore::load(const QString &id, DubbingTimingProfile &profile)
{
    QFile file(profilePath(id));
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return false;
    profile = DubbingTimingProfile::fromVariantMap(document.object().toVariantMap());
    return profile.id == id && profile.phonemesPerSecond > 0.0
        && profile.normalizerVersion == QStringLiteral("phoneme-v1");
}

bool DubbingTimingProfileStore::save(const DubbingTimingProfile &profile, QString *error)
{
    if (profile.id.isEmpty()) {
        if (error) *error = QStringLiteral("Timing profile id is empty.");
        return false;
    }
    const QString path = profilePath(profile.id);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = QStringLiteral("Cannot create timing profile directory.");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(profile.toVariantMap())).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace LAStudio
