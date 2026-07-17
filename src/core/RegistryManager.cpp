#include "RegistryManager.h"

#include "CatalogManager.h"
#include "Logger.h"
#include "PathUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariantMap>

namespace LAStudio {

namespace {

constexpr const char *kRegistrySourceId = "bundled";

QString jsonFromVariant(const QVariant &value)
{
    return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
}

QVariant variantFromJson(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        return {};
    }
    return doc.toVariant();
}

QStringList splitSqlStatements(const QString &sql)
{
    QStringList statements;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for (const QChar ch : sql) {
        if (ch == QLatin1Char('\'') && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (ch == QLatin1Char('"') && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        }

        if (ch == QLatin1Char(';') && !inSingleQuote && !inDoubleQuote) {
            const QString trimmed = current.trimmed();
            if (!trimmed.isEmpty()) {
                statements.append(trimmed);
            }
            current.clear();
        } else {
            current.append(ch);
        }
    }

    const QString trimmed = current.trimmed();
    if (!trimmed.isEmpty()) {
        statements.append(trimmed);
    }
    return statements;
}

bool execStatement(QSqlDatabase &db, const QString &sql, const QString &context)
{
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("%1 failed: %2").arg(context, query.lastError().text()));
        return false;
    }
    return true;
}

bool execPrepared(QSqlQuery &query, const QString &context)
{
    if (!query.exec()) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("%1 failed: %2").arg(context, query.lastError().text()));
        return false;
    }
    return true;
}

QString readFirstAvailableFile(const QStringList &paths)
{
    for (const QString &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        return QString::fromUtf8(file.readAll());
    }
    return {};
}

QString acceleratorFromRuntime(const QVariantMap &runtime)
{
    const QString haystack = QStringList{
        runtime.value(QStringLiteral("id")).toString(),
        runtime.value(QStringLiteral("label")).toString(),
        runtime.value(QStringLiteral("asset")).toString(),
        runtime.value(QStringLiteral("name")).toString()
    }.join(QLatin1Char(' ')).toLower();

    if (haystack.contains(QStringLiteral("cuda"))) return QStringLiteral("cuda");
    if (haystack.contains(QStringLiteral("vulkan"))) return QStringLiteral("vulkan");
    if (haystack.contains(QStringLiteral("hip")) || haystack.contains(QStringLiteral("radeon")))
        return QStringLiteral("hip");
    if (haystack.contains(QStringLiteral("sycl"))) return QStringLiteral("sycl");
    if (haystack.contains(QStringLiteral("openvino"))) return QStringLiteral("openvino");
    if (haystack.contains(QStringLiteral("directml"))) return QStringLiteral("directml");
    return QStringLiteral("cpu");
}

QString runtimeTypeForFamily(const QVariantMap &family)
{
    const QVariantList capabilities = family.value(QStringLiteral("capabilities")).toList();
    if (capabilities.contains(QStringLiteral("tts")) && capabilities.contains(QStringLiteral("stt"))) {
        return QStringLiteral("mixed");
    }
    if (capabilities.contains(QStringLiteral("tts"))) return QStringLiteral("tts");
    if (capabilities.contains(QStringLiteral("stt"))) return QStringLiteral("stt");
    if (capabilities.contains(QStringLiteral("forced-alignment"))) return QStringLiteral("alignment");
    if (capabilities.contains(QStringLiteral("voice-cloning"))) return QStringLiteral("voice-cloning");
    if (capabilities.contains(QStringLiteral("voice-design"))) return QStringLiteral("tts");
    return QStringLiteral("mixed");
}

} // namespace

RegistryManager::RegistryManager(QObject *parent)
    : QObject(parent)
    , m_databasePath(PathUtils::dataDir() + QStringLiteral("/registry.sqlite"))
    , m_connectionName(QStringLiteral("LAStudioRegistry_%1").arg(reinterpret_cast<quintptr>(this)))
{
}

void RegistryManager::initializeFromCatalog(CatalogManager *catalog)
{
    m_catalog = catalog;
    if (!m_catalog) {
        emit errorOccurred(QStringLiteral("Catalog is unavailable"));
        return;
    }

    connect(m_catalog, &CatalogManager::catalogUpdated,
            this, &RegistryManager::refreshFromCatalog);

    refreshFromCatalog();
}

void RegistryManager::refreshFromCatalog()
{
    if (!m_catalog) return;
    if (!openDatabase() || !ensureSchema()) {
        emit errorOccurred(QStringLiteral("Failed to initialize registry database"));
        return;
    }

    if (!importCatalog(m_catalog->modelCategories(),
                       m_catalog->ttsFamilies(),
                       m_catalog->sttFamilies(),
                       m_catalog->version())) {
        emit errorOccurred(QStringLiteral("Failed to import catalog into registry"));
        return;
    }

    if (reloadCachedViews()) {
        emit registryUpdated();
    }
}

bool RegistryManager::openDatabase()
{
    QDir().mkpath(QFileInfo(m_databasePath).absolutePath());

    QSqlDatabase db;
    if (QSqlDatabase::contains(m_connectionName)) {
        db = QSqlDatabase::database(m_connectionName);
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(m_databasePath);
    }

    if (!db.open()) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("Failed to open registry database: %1").arg(db.lastError().text()));
        return false;
    }

    execStatement(db, QStringLiteral("PRAGMA foreign_keys = ON"), QStringLiteral("enable foreign keys"));
    execStatement(db, QStringLiteral("PRAGMA journal_mode = WAL"), QStringLiteral("enable WAL"));
    return true;
}

bool RegistryManager::ensureSchema()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const QString schema = readFirstAvailableFile({
        QStringLiteral(":/LAStudio/data/registry_schema.sql"),
        QStringLiteral(":/data/registry_schema.sql"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/data/registry_schema.sql"),
        QDir::currentPath() + QStringLiteral("/data/registry_schema.sql")
    });

    if (schema.isEmpty()) {
        Logger::error(QStringLiteral("RegistryManager"), QStringLiteral("Registry schema file is missing"));
        return false;
    }

    for (const QString &statement : splitSqlStatements(schema)) {
        if (!execStatement(db, statement, QStringLiteral("apply registry schema"))) {
            return false;
        }
    }
    return true;
}

bool RegistryManager::importCatalog(const QVariantList &modelCategories,
                                    const QVariantList &ttsFamilies,
                                    const QVariantList &sttFamilies,
                                    const QString &catalogVersion)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("Failed to start registry import transaction: %1").arg(db.lastError().text()));
        return false;
    }

    auto rollback = [&db]() {
        db.rollback();
        return false;
    };

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO catalog_sources (id, kind, version, uri, imported_at) "
            "VALUES (?, 'bundled', ?, 'catalog.json', CURRENT_TIMESTAMP) "
            "ON CONFLICT(id) DO UPDATE SET version = excluded.version, imported_at = CURRENT_TIMESTAMP"));
        query.addBindValue(QString::fromLatin1(kRegistrySourceId));
        query.addBindValue(catalogVersion);
        if (!execPrepared(query, QStringLiteral("upsert catalog source"))) return rollback();
    }

    const QStringList cleanup = {
        QStringLiteral("DELETE FROM catalog_model_categories WHERE source_id = 'bundled'"),
        QStringLiteral("DELETE FROM model_families WHERE source_id = 'bundled'"),
        QStringLiteral("DELETE FROM runtime_engines WHERE source_id = 'bundled'")
    };
    for (const QString &sql : cleanup) {
        if (!execStatement(db, sql, QStringLiteral("clean bundled registry rows"))) return rollback();
    }

    auto upsertCapability = [&db](const QString &capability) -> bool {
        if (capability.isEmpty()) return true;
        QSqlQuery query(db);
        query.prepare(QStringLiteral("INSERT OR IGNORE INTO capabilities (id) VALUES (?)"));
        query.addBindValue(capability);
        return execPrepared(query, QStringLiteral("upsert capability"));
    };

    for (int i = 0; i < modelCategories.size(); ++i) {
        const QVariantMap category = modelCategories.at(i).toMap();
        const QString categoryId = category.value(QStringLiteral("id")).toString();
        if (categoryId.isEmpty()) continue;

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO catalog_model_categories "
            "(id, source_id, display_name, title, description, sort_order, metadata_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)"));
        query.addBindValue(categoryId);
        query.addBindValue(QString::fromLatin1(kRegistrySourceId));
        query.addBindValue(category.value(QStringLiteral("name")).toString());
        const QString categoryName = category.value(QStringLiteral("name")).toString();
        const QString categoryTitle = category.value(QStringLiteral("title")).toString().isEmpty()
            ? categoryName
            : category.value(QStringLiteral("title")).toString();
        query.addBindValue(categoryTitle);
        query.addBindValue(category.value(QStringLiteral("description")).toString());
        query.addBindValue(i);
        query.addBindValue(jsonFromVariant(category));
        if (!execPrepared(query, QStringLiteral("insert model category"))) return rollback();

        const QVariantList capabilities = category.value(QStringLiteral("capabilities")).toList();
        for (const QVariant &capabilityValue : capabilities) {
            const QString capability = capabilityValue.toString();
            if (!upsertCapability(capability)) return rollback();

            QSqlQuery linkQuery(db);
            linkQuery.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO catalog_model_category_capabilities (category_id, capability_id) VALUES (?, ?)"));
            linkQuery.addBindValue(categoryId);
            linkQuery.addBindValue(capability);
            if (!execPrepared(linkQuery, QStringLiteral("link category capability"))) return rollback();
        }
    }

    const QVariantList allFamilies = ttsFamilies + sttFamilies;
    for (const QVariant &familyValue : allFamilies) {
        const QVariantMap family = familyValue.toMap();
        const QString familyId = family.value(QStringLiteral("id")).toString();
        if (familyId.isEmpty()) continue;

        QSqlQuery familyQuery(db);
        familyQuery.prepare(QStringLiteral(
            "INSERT INTO model_families "
            "(id, source_id, local_dir, provider_model_id, title, subtitle, description, accent, studio_json, metadata_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        familyQuery.addBindValue(familyId);
        familyQuery.addBindValue(QString::fromLatin1(kRegistrySourceId));
        familyQuery.addBindValue(family.value(QStringLiteral("localDir")).toString());
        familyQuery.addBindValue(family.value(QStringLiteral("modelId")).toString());
        familyQuery.addBindValue(family.value(QStringLiteral("title")).toString());
        familyQuery.addBindValue(family.value(QStringLiteral("subtitle")).toString());
        familyQuery.addBindValue(family.value(QStringLiteral("description")).toString());
        familyQuery.addBindValue(family.value(QStringLiteral("accent")).toString());
        familyQuery.addBindValue(jsonFromVariant(family.value(QStringLiteral("studio"))));
        familyQuery.addBindValue(jsonFromVariant(family));
        if (!execPrepared(familyQuery, QStringLiteral("insert model family"))) return rollback();

        const QVariantList capabilities = family.value(QStringLiteral("capabilities")).toList();
        for (const QVariant &capabilityValue : capabilities) {
            const QString capability = capabilityValue.toString();
            if (!upsertCapability(capability)) return rollback();

            QSqlQuery linkQuery(db);
            linkQuery.prepare(QStringLiteral("INSERT OR IGNORE INTO family_capabilities (family_id, capability_id) VALUES (?, ?)"));
            linkQuery.addBindValue(familyId);
            linkQuery.addBindValue(capability);
            if (!execPrepared(linkQuery, QStringLiteral("link family capability"))) return rollback();
        }

        const QVariantList requirements = family.value(QStringLiteral("requiredFiles")).toList();
        for (int reqIndex = 0; reqIndex < requirements.size(); ++reqIndex) {
            const QVariantMap req = requirements.at(reqIndex).toMap();
            QSqlQuery reqQuery(db);
            reqQuery.prepare(QStringLiteral(
                "INSERT INTO family_requirements "
                "(family_id, role, display_name, purpose, default_filename, required, min_count, sort_order, metadata_json) "
                "VALUES (?, ?, ?, ?, ?, 1, 1, ?, ?)"));
            reqQuery.addBindValue(familyId);
            reqQuery.addBindValue(req.value(QStringLiteral("role")).toString());
            reqQuery.addBindValue(req.value(QStringLiteral("name")).toString());
            reqQuery.addBindValue(req.value(QStringLiteral("purpose")).toString());
            reqQuery.addBindValue(req.value(QStringLiteral("file")).toString());
            reqQuery.addBindValue(reqIndex);
            reqQuery.addBindValue(jsonFromVariant(req));
            if (!execPrepared(reqQuery, QStringLiteral("insert family requirement"))) return rollback();

            const int requirementId = reqQuery.lastInsertId().toInt();
            QVariantList candidates = req.value(QStringLiteral("candidates")).toList();
            if (candidates.isEmpty()) {
                candidates.append(req.value(QStringLiteral("file")));
            }
            for (int c = 0; c < candidates.size(); ++c) {
                const QString filename = candidates.at(c).toString();
                if (filename.isEmpty()) continue;
                QSqlQuery candidateQuery(db);
                candidateQuery.prepare(QStringLiteral(
                    "INSERT INTO requirement_candidates "
                    "(requirement_id, provider_model_id, filename, label, size_label, is_default, sort_order, metadata_json) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
                candidateQuery.addBindValue(requirementId);
                const QString providerModelId = req.value(QStringLiteral("modelId")).toString().isEmpty()
                    ? family.value(QStringLiteral("modelId")).toString()
                    : req.value(QStringLiteral("modelId")).toString();
                candidateQuery.addBindValue(providerModelId);
                candidateQuery.addBindValue(filename);
                candidateQuery.addBindValue(req.value(QStringLiteral("name")).toString());
                candidateQuery.addBindValue(req.value(QStringLiteral("size")).toString());
                candidateQuery.addBindValue(filename == req.value(QStringLiteral("file")).toString() ? 1 : 0);
                candidateQuery.addBindValue(c);
                candidateQuery.addBindValue(jsonFromVariant(req));
                if (!execPrepared(candidateQuery, QStringLiteral("insert requirement candidate"))) return rollback();
            }
        }

        const QVariantList runtimes = family.value(QStringLiteral("runtimes")).toList();
        for (int runtimeIndex = 0; runtimeIndex < runtimes.size(); ++runtimeIndex) {
            const QVariantMap runtime = runtimes.at(runtimeIndex).toMap();
            const QString runtimeId = runtime.value(QStringLiteral("id")).toString();
            if (runtimeId.isEmpty()) continue;

            QSqlQuery runtimeQuery(db);
            runtimeQuery.prepare(QStringLiteral(
                "INSERT INTO runtime_engines "
                "(id, source_id, engine_family, display_name, backend, type, library_path, metadata_json) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                "ON CONFLICT(id) DO UPDATE SET "
                "source_id = excluded.source_id, "
                "engine_family = excluded.engine_family, "
                "display_name = excluded.display_name, "
                "backend = excluded.backend, "
                "type = excluded.type, "
                "library_path = excluded.library_path, "
                "metadata_json = excluded.metadata_json"));
            runtimeQuery.addBindValue(runtimeId);
            runtimeQuery.addBindValue(QString::fromLatin1(kRegistrySourceId));
            runtimeQuery.addBindValue(runtime.value(QStringLiteral("engineFamily")).toString());
            runtimeQuery.addBindValue(runtime.value(QStringLiteral("name")).toString());
            runtimeQuery.addBindValue(runtime.value(QStringLiteral("backend")).toString());
            const QString runtimeType = runtime.value(QStringLiteral("type")).toString().isEmpty()
                ? runtimeTypeForFamily(family)
                : runtime.value(QStringLiteral("type")).toString();
            runtimeQuery.addBindValue(runtimeType == QStringLiteral("alignment") ? QStringLiteral("stt") : runtimeType);
            runtimeQuery.addBindValue(runtime.value(QStringLiteral("library")).toString());
            runtimeQuery.addBindValue(jsonFromVariant(runtime));
            if (!execPrepared(runtimeQuery, QStringLiteral("insert runtime engine"))) return rollback();

            QSqlQuery versionQuery(db);
            versionQuery.prepare(QStringLiteral(
                "INSERT INTO runtime_versions "
                "(runtime_id, version, platform, arch, accelerator, asset_name, source_base_url, metadata_json) "
                "VALUES (?, ?, 'win', 'x86_64', ?, ?, ?, ?) "
                "ON CONFLICT(runtime_id, version, platform, arch, accelerator) DO UPDATE SET "
                "asset_name = excluded.asset_name, "
                "source_base_url = excluded.source_base_url, "
                "metadata_json = excluded.metadata_json"));
            versionQuery.addBindValue(runtimeId);
            versionQuery.addBindValue(runtime.value(QStringLiteral("version")).toString());
            const QString accelerator = acceleratorFromRuntime(runtime);
            versionQuery.addBindValue(accelerator);
            versionQuery.addBindValue(runtime.value(QStringLiteral("asset")).toString());
            versionQuery.addBindValue(runtime.value(QStringLiteral("source")).toString());
            versionQuery.addBindValue(jsonFromVariant(runtime));
            if (!execPrepared(versionQuery, QStringLiteral("insert runtime version"))) return rollback();

            QSqlQuery runtimeVersionIdQuery(db);
            runtimeVersionIdQuery.prepare(QStringLiteral(
                "SELECT id FROM runtime_versions "
                "WHERE runtime_id = ? AND version = ? AND platform = 'win' AND arch = 'x86_64' AND accelerator = ?"));
            runtimeVersionIdQuery.addBindValue(runtimeId);
            runtimeVersionIdQuery.addBindValue(runtime.value(QStringLiteral("version")).toString());
            runtimeVersionIdQuery.addBindValue(accelerator);
            if (!execPrepared(runtimeVersionIdQuery, QStringLiteral("select runtime version id"))) return rollback();
            if (!runtimeVersionIdQuery.next()) {
                Logger::error(QStringLiteral("RegistryManager"), QStringLiteral("Runtime version id was not found after upsert"));
                return rollback();
            }
            const int runtimeVersionId = runtimeVersionIdQuery.value(0).toInt();

            QSqlQuery deleteDependenciesQuery(db);
            deleteDependenciesQuery.prepare(QStringLiteral("DELETE FROM runtime_dependencies WHERE runtime_version_id = ?"));
            deleteDependenciesQuery.addBindValue(runtimeVersionId);
            if (!execPrepared(deleteDependenciesQuery, QStringLiteral("replace runtime dependencies"))) return rollback();

            const QVariantList dependencies = runtime.value(QStringLiteral("dependencyDownloads")).toList();
            for (const QVariant &dependencyValue : dependencies) {
                const QVariantMap dependency = dependencyValue.toMap();
                QSqlQuery depQuery(db);
                depQuery.prepare(QStringLiteral(
                    "INSERT INTO runtime_dependencies "
                    "(runtime_version_id, dependency_id, filename, url, metadata_json) "
                    "VALUES (?, ?, ?, ?, ?)"));
                depQuery.addBindValue(runtimeVersionId);
                depQuery.addBindValue(dependency.value(QStringLiteral("dependency")).toString());
                depQuery.addBindValue(dependency.value(QStringLiteral("filename")).toString());
                depQuery.addBindValue(dependency.value(QStringLiteral("url")).toString());
                depQuery.addBindValue(jsonFromVariant(dependency));
                if (!execPrepared(depQuery, QStringLiteral("insert runtime dependency"))) return rollback();
            }

            QSqlQuery familyRuntimeQuery(db);
            familyRuntimeQuery.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO family_runtimes (family_id, runtime_id, sort_order) VALUES (?, ?, ?)"));
            familyRuntimeQuery.addBindValue(familyId);
            familyRuntimeQuery.addBindValue(runtimeId);
            familyRuntimeQuery.addBindValue(runtimeIndex);
            if (!execPrepared(familyRuntimeQuery, QStringLiteral("link family runtime"))) return rollback();
        }
    }

    if (!db.commit()) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("Failed to commit registry import: %1").arg(db.lastError().text()));
        return false;
    }

    Logger::info(QStringLiteral("RegistryManager"),
                 QStringLiteral("Imported catalog into registry: %1 categories, %2 families")
                     .arg(modelCategories.size())
                     .arg(allFamilies.size()));
    return true;
}

bool RegistryManager::reloadCachedViews()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QVariantList categories;
    QVariantList ttsFamilies;
    QVariantList sttFamilies;

    QSqlQuery categoryQuery(db);
    if (!categoryQuery.exec(QStringLiteral(
            "SELECT metadata_json FROM catalog_model_categories WHERE source_id = 'bundled' ORDER BY sort_order ASC"))) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("Failed to load category view: %1").arg(categoryQuery.lastError().text()));
        return false;
    }
    while (categoryQuery.next()) {
        const QVariant value = variantFromJson(categoryQuery.value(0).toString());
        if (value.isValid()) categories.append(value);
    }

    QSqlQuery familyQuery(db);
    if (!familyQuery.exec(QStringLiteral(
            "SELECT f.metadata_json "
            "FROM model_families f "
            "WHERE f.source_id = 'bundled' "
            "AND EXISTS ("
            "  SELECT 1 FROM family_capabilities c "
            "  WHERE c.family_id = f.id "
            "    AND c.capability_id IN ('tts', 'voice-cloning', 'voice-design')"
            ") "
            "ORDER BY f.rowid ASC"))) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("Failed to load TTS family view: %1").arg(familyQuery.lastError().text()));
        return false;
    }
    while (familyQuery.next()) {
        const QVariant value = variantFromJson(familyQuery.value(0).toString());
        if (value.isValid()) ttsFamilies.append(value);
    }

    QSqlQuery sttQuery(db);
    if (!sttQuery.exec(QStringLiteral(
            "SELECT f.metadata_json "
            "FROM model_families f "
            "JOIN family_capabilities c ON c.family_id = f.id "
            "WHERE f.source_id = 'bundled' AND c.capability_id IN ('stt', 'forced-alignment', 'translation', 'voice-isolation') "
            "ORDER BY f.rowid ASC"))) {
        Logger::error(QStringLiteral("RegistryManager"),
                      QStringLiteral("Failed to load STT family view: %1").arg(sttQuery.lastError().text()));
        return false;
    }
    while (sttQuery.next()) {
        const QVariant value = variantFromJson(sttQuery.value(0).toString());
        if (value.isValid()) sttFamilies.append(value);
    }

    m_modelCategories = categories;
    m_ttsFamilies = ttsFamilies;
    m_sttFamilies = sttFamilies;
    return true;
}

QString RegistryManager::connectionName() const
{
    return m_connectionName;
}

} // namespace LAStudio
