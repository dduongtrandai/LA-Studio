#include "CatalogManager.h"
#include "PathUtils.h"
#include "Logger.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QVersionNumber>
#include <QCoreApplication>
#include <QVariantMap>

namespace LAStudio {

namespace {

QByteArray readFirstAvailableFile(const QStringList &paths, QString *loadedPath = nullptr)
{
    for (const QString &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        if (loadedPath) {
            *loadedPath = path;
        }
        return file.readAll();
    }

    return {};
}

QJsonDocument documentFromJson(const QByteArray &data, const QString &source)
{
    if (data.isEmpty()) {
        Logger::error("CatalogManager", QString("Failed to parse %1 catalog JSON: file is empty")
                      .arg(source));
        return {};
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("CatalogManager", QString("Failed to parse %1 catalog JSON: %2")
                      .arg(source, error.errorString()));
        return {};
    }
    return doc;
}

bool hasNonEmptyArray(const QJsonObject &object, const QString &key)
{
    return object.value(key).isArray() && !object.value(key).toArray().isEmpty();
}

QByteArray catalogWithBundledFallback(const QByteArray &candidateData,
                                      const QByteArray &bundledData,
                                      const QString &candidateName)
{
    QJsonDocument candidateDoc = documentFromJson(candidateData, candidateName);
    if (!candidateDoc.isObject()) {
        return bundledData;
    }

    QJsonDocument bundledDoc = documentFromJson(bundledData, QStringLiteral("bundled"));
    if (!bundledDoc.isObject()) {
        return candidateData;
    }

    QJsonObject candidate = candidateDoc.object();
    const QJsonObject bundled = bundledDoc.object();
    bool changed = false;

    const QStringList requiredArrays = {
        QStringLiteral("modelCategories"),
        QStringLiteral("ttsFamilies"),
        QStringLiteral("sttFamilies")
    };

    for (const QString &key : requiredArrays) {
        if (!hasNonEmptyArray(candidate, key) && hasNonEmptyArray(bundled, key)) {
            candidate.insert(key, bundled.value(key).toArray());
            changed = true;
        }
    }

    if (!changed) {
        return candidateData;
    }

    Logger::info("CatalogManager", QString("Patched incomplete %1 catalog with bundled fallback data")
                 .arg(candidateName));
    return QJsonDocument(candidate).toJson(QJsonDocument::Compact);
}

bool isBundledNewer(const QString &bundledVersion, const QString &candidateVersion)
{
    const QVersionNumber bundled = QVersionNumber::fromString(bundledVersion);
    const QVersionNumber candidate = QVersionNumber::fromString(candidateVersion);
    if (!bundled.isNull() && !candidate.isNull()) {
        return QVersionNumber::compare(bundled, candidate) > 0;
    }
    return bundledVersion > candidateVersion;
}

QVariantMap lastudioPickForFamily(const QVariantList &modelPicks, const QVariantMap &family)
{
    const QString familyId = family.value(QStringLiteral("id")).toString();
    const QString modelId = family.value(QStringLiteral("modelId")).toString();
    const QString realId = family.value(QStringLiteral("realId")).toString();

    for (const QVariant &collectionValue : modelPicks) {
        const QVariantMap collection = collectionValue.toMap();
        if (collection.value(QStringLiteral("id")).toString() != QStringLiteral("lastudio-picks")) {
            continue;
        }

        const QVariantList items = collection.value(QStringLiteral("items")).toList();
        for (const QVariant &pickValue : items) {
            const QVariantMap pick = pickValue.toMap();
            const QString pickFamilyId = pick.value(QStringLiteral("familyId")).toString();
            const QString pickModelId = pick.value(QStringLiteral("modelId")).toString();
            const bool familyMatches = !familyId.isEmpty() && pickFamilyId == familyId;
            const bool modelMatches = !pickModelId.isEmpty() && (pickModelId == modelId || pickModelId == realId);
            if (familyMatches || modelMatches) {
                QVariantMap marker = pick;
                marker.insert(QStringLiteral("collectionId"), collection.value(QStringLiteral("id")));
                marker.insert(QStringLiteral("collectionTitle"), collection.value(QStringLiteral("title")));
                marker.insert(QStringLiteral("label"), QStringLiteral("LA Studio Pick"));
                return marker;
            }
        }
    }

    return {};
}

void applyLastudioPickMetadata(QVariantList &families, const QVariantList &modelPicks)
{
    for (int i = 0; i < families.size(); ++i) {
        QVariantMap family = families.at(i).toMap();
        const QVariantMap pick = lastudioPickForFamily(modelPicks, family);
        if (pick.isEmpty()) {
            continue;
        }

        family.insert(QStringLiteral("isLastudioPick"), true);
        family.insert(QStringLiteral("pickLabel"), pick.value(QStringLiteral("label")));
        family.insert(QStringLiteral("pickReason"), pick.value(QStringLiteral("reason")));
        family.insert(QStringLiteral("pickCollectionId"), pick.value(QStringLiteral("collectionId")));
        family.insert(QStringLiteral("pickCollectionTitle"), pick.value(QStringLiteral("collectionTitle")));
        families[i] = family;
    }
}

}

CatalogManager::CatalogManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    loadLocalCatalog();
}

void CatalogManager::loadLocalCatalog()
{
    // Always load bundled catalog as baseline
    QString bundledPath;
    QByteArray bundledData = readFirstAvailableFile({
        QStringLiteral(":/LAStudio/data/catalog.json"),
        QStringLiteral(":/data/catalog.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/data/catalog.json"),
        QDir::currentPath() + QStringLiteral("/data/catalog.json")
    }, &bundledPath);

    if (bundledData.isEmpty()) {
        Logger::error("CatalogManager", "Failed to load bundled catalog from resources");
    } else {
        Logger::info("CatalogManager", "Loaded bundled catalog from " + bundledPath);
    }

    QString cachePath = PathUtils::dataDir() + QStringLiteral("/catalog.json");
    QFile cacheFile(cachePath);

    if (cacheFile.exists() && cacheFile.open(QIODevice::ReadOnly)) {
        QByteArray cacheData = cacheFile.readAll();
        cacheFile.close();

        QJsonDocument bundledDoc = documentFromJson(bundledData, QStringLiteral("bundled"));
        QJsonDocument cacheDoc = documentFromJson(cacheData, QStringLiteral("cached"));

        const QJsonObject bundledObj = bundledDoc.object();
        const QJsonObject cacheObj = cacheDoc.object();
        QString bundledVer = bundledObj.value("version").toString();
        QString cacheVer = cacheObj.value("version").toString();
        bool bundledHasTts = hasNonEmptyArray(bundledObj, QStringLiteral("ttsFamilies"));
        bool cacheHasTts = hasNonEmptyArray(cacheObj, QStringLiteral("ttsFamilies"));
        const int bundledTtsCount = bundledObj.value(QStringLiteral("ttsFamilies")).toArray().size();
        const int cacheTtsCount = cacheObj.value(QStringLiteral("ttsFamilies")).toArray().size();
        const bool bundledHasMoreTtsFamilies = bundledTtsCount > cacheTtsCount;

        // Use bundled if it's newer or if it is more complete than cache.
        if (isBundledNewer(bundledVer, cacheVer) ||
            (bundledHasTts && !cacheHasTts) ||
            bundledHasMoreTtsFamilies) {
            Logger::info("CatalogManager", "Bundled catalog is newer or more complete. Using bundled.");
            parseCatalog(bundledData);
            saveToCache(bundledData); // Update cache with newer bundled version
        } else {
            cacheData = catalogWithBundledFallback(cacheData, bundledData, QStringLiteral("cached"));
            Logger::info("CatalogManager", "Loaded catalog from cache (version " + cacheVer + ")");
            parseCatalog(cacheData);
            if (!cacheHasTts && bundledHasTts) {
                saveToCache(cacheData);
            }
        }
    } else {
        Logger::info("CatalogManager", "No cached catalog found. Using bundled version.");
        parseCatalog(bundledData);
    }
}

void CatalogManager::parseCatalog(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        Logger::error("CatalogManager", "Failed to parse catalog JSON: " + error.errorString());
        return;
    }

    QJsonObject obj = doc.object();
    m_version = obj.value(QStringLiteral("version")).toString();
    const QVariantList modelPicks = obj.value("modelPicks").toArray().toVariantList();
    m_modelCategories = obj.value("modelCategories").toArray().toVariantList();
    m_ttsFamilies = obj.value("ttsFamilies").toArray().toVariantList();
    m_sttFamilies = obj.value("sttFamilies").toArray().toVariantList();

    applyLastudioPickMetadata(m_ttsFamilies, modelPicks);
    applyLastudioPickMetadata(m_sttFamilies, modelPicks);

    // Clear the language sets cache when catalog updates, to ensure fresh copies are loaded
    m_languageSetsCache.clear();

    Logger::info("CatalogManager", QString("Catalog parsed: %1 categories, %2 TTS families, %3 STT families")
                 .arg(m_modelCategories.size()).arg(m_ttsFamilies.size()).arg(m_sttFamilies.size()));

    emit catalogUpdated();
}

void CatalogManager::fetchRemoteCatalog(const QString &url)
{
    if (url.isEmpty() || m_isUpdating) return;

    m_isUpdating = true;
    emit isUpdatingChanged();

    QNetworkRequest request((QUrl(url)));
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_isUpdating = false;
        emit isUpdatingChanged();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QByteArray bundledData = readFirstAvailableFile({
                QStringLiteral(":/LAStudio/data/catalog.json"),
                QStringLiteral(":/data/catalog.json"),
                QCoreApplication::applicationDirPath() + QStringLiteral("/data/catalog.json"),
                QDir::currentPath() + QStringLiteral("/data/catalog.json")
            });
            if (!bundledData.isEmpty()) {
                data = catalogWithBundledFallback(data, bundledData, QStringLiteral("remote"));
            }
            parseCatalog(data);
            saveToCache(data);
            Logger::info("CatalogManager", "Remote catalog updated successfully");
        } else {
            Logger::error("CatalogManager", "Failed to fetch remote catalog: " + reply->errorString());
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    });
}

QVariantList CatalogManager::languageSet(const QString &setId)
{
    if (setId.isEmpty()) {
        return {};
    }

    if (m_languageSetsCache.contains(setId)) {
        return m_languageSetsCache.value(setId);
    }

    QString relativePath = QStringLiteral("/data/language-sets/") + setId + QStringLiteral(".json");
    
    QStringList paths = {
        QStringLiteral(":/LAStudio") + relativePath,
        QStringLiteral(":") + relativePath,
        QCoreApplication::applicationDirPath() + relativePath,
        QDir::currentPath() + relativePath
    };

    QByteArray data = readFirstAvailableFile(paths);
    if (data.isEmpty()) {
        Logger::error("CatalogManager", QString("Failed to load language set: %1").arg(setId));
        return {};
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        Logger::error("CatalogManager", QString("Failed to parse language set %1 JSON: %2")
                      .arg(setId, error.errorString()));
        return {};
    }

    if (!doc.isObject()) {
        return {};
    }

    QVariantList list = doc.object().value("languages").toArray().toVariantList();
    m_languageSetsCache.insert(setId, list);
    return list;
}

void CatalogManager::saveToCache(const QByteArray &data)
{
    QString cacheDir = PathUtils::dataDir();
    QDir().mkpath(cacheDir);
    
    QFile file(cacheDir + "/catalog.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
    }
}

} // namespace LAStudio
