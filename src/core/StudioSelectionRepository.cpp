#include "StudioSelectionRepository.h"
#include "core/Settings.h"
#include "core/Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

namespace LAStudio {

StudioSelectionRepository::StudioSelectionRepository(const QString &connectionName, QObject *parent)
    : QObject(parent)
    , m_connectionName(connectionName)
{
}

QSqlDatabase StudioSelectionRepository::db() const
{
    return QSqlDatabase::database(m_connectionName);
}

StudioConfiguration StudioSelectionRepository::selectionFor(const QString &capabilityId) const
{
    QSqlDatabase database = db();
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT family_id, runtime_id, runtime_version, selected_files_json FROM active_capability_selections "
        "WHERE capability_id = ?"));
    query.addBindValue(capabilityId);

    if (!query.exec()) {
        Logger::error(QStringLiteral("StudioSelectionRepository"),
                      QStringLiteral("Failed to fetch selection for %1: %2").arg(capabilityId, query.lastError().text()));
        return {};
    }

    if (query.next()) {
        StudioConfiguration config;
        config.capabilityId = capabilityId;
        config.familyId = query.value(0).toString();
        config.runtimeId = query.value(1).toString();
        config.runtimeVersion = query.value(2).toString();
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(query.value(3).toString().toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            config.selectedFiles = doc.object().toVariantMap();
        }
        return config;
    }

    return {};
}

void StudioSelectionRepository::saveActiveSelection(const StudioConfiguration &selection)
{
    if (!selection.isValid()) return;

    QSqlDatabase database = db();
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO active_capability_selections (capability_id, family_id, runtime_id, runtime_version, selected_files_json, updated_at) "
        "VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(capability_id) DO UPDATE SET "
        "  family_id = excluded.family_id, "
        "  runtime_id = excluded.runtime_id, "
        "  runtime_version = excluded.runtime_version, "
        "  selected_files_json = excluded.selected_files_json, "
        "  updated_at = CURRENT_TIMESTAMP"));

    query.addBindValue(selection.capabilityId);
    query.addBindValue(selection.familyId);
    query.addBindValue(selection.runtimeId);
    query.addBindValue(selection.runtimeVersion);
    
    QString filesJson = QString::fromUtf8(QJsonDocument::fromVariant(selection.selectedFiles).toJson(QJsonDocument::Compact));
    query.addBindValue(filesJson);

    if (!query.exec()) {
        Logger::error(QStringLiteral("StudioSelectionRepository"),
                      QStringLiteral("Failed to save selection for %1: %2").arg(selection.capabilityId, query.lastError().text()));
    }
}

void StudioSelectionRepository::clearActiveSelection(const QString &capabilityId)
{
    QSqlDatabase database = db();
    QSqlQuery query(database);
    query.prepare(QStringLiteral("DELETE FROM active_capability_selections WHERE capability_id = ?"));
    query.addBindValue(capabilityId);

    if (!query.exec()) {
        Logger::error(QStringLiteral("StudioSelectionRepository"),
                      QStringLiteral("Failed to clear selection for %1: %2").arg(capabilityId, query.lastError().text()));
    }
}

void StudioSelectionRepository::migrateLegacySelectionsIfNeeded(Settings *settings)
{
    if (!settings) return;

    auto normalizeRuntimeId = [](QString runtimeId) {
        if (runtimeId == QStringLiteral("sherpa-onnx-source-separation-win-x86_64-cpu")) {
            return QStringLiteral("sherpa-onnx-win-x86_64-cpu");
        }
        return runtimeId;
    };

    auto normalizeFamilyId = [](QString familyId) {
        if (familyId == QStringLiteral("sherpa-onnx-source-separation")) {
            return QStringLiteral("sherpa-onnx-uvr-vocals-ft");
        }
        return familyId;
    };

    auto isCatalogSelection = [this](const StudioConfiguration &selection) {
        if (selection.familyId.isEmpty()) return false;
        QSqlQuery query(db());
        query.prepare(QStringLiteral(
            "SELECT 1 FROM model_families f "
            "JOIN family_capabilities fc ON fc.family_id = f.id "
            "LEFT JOIN family_runtimes fr ON fr.family_id = f.id AND fr.runtime_id = ? "
            "WHERE f.id = ? AND fc.capability_id = ? "
            "AND (? = '' OR fr.runtime_id IS NOT NULL) LIMIT 1"));
        query.addBindValue(selection.runtimeId);
        query.addBindValue(selection.familyId);
        query.addBindValue(selection.capabilityId);
        query.addBindValue(selection.runtimeId);
        return query.exec() && query.next();
    };

    QStringList capabilities = { QStringLiteral("stt"), QStringLiteral("tts"), QStringLiteral("voice-cloning") };
    for (const QString &cap : capabilities) {
        StudioConfiguration existing = selectionFor(cap);
        if (existing.isValid()) {
            continue; // Already migrated or has active selection
        }

        StudioConfiguration legacyConfig;
        legacyConfig.capabilityId = cap;

        if (cap == QStringLiteral("stt")) {
            legacyConfig.familyId = settings->selectedSttFamily();
            legacyConfig.runtimeId = settings->selectedSttRuntime();
            legacyConfig.runtimeVersion = settings->selectedSttRuntimeVersion();
            QString modelFile = settings->selectedSttModelFile();
            if (!modelFile.isEmpty()) {
                legacyConfig.selectedFiles.insert(QStringLiteral("model"), modelFile);
            }
        } else if (cap == QStringLiteral("tts")) {
            legacyConfig.familyId = settings->selectedTtsFamily();
            legacyConfig.runtimeId = settings->selectedTtsRuntime();
            legacyConfig.runtimeVersion = settings->selectedTtsRuntimeVersion();
        } else if (cap == QStringLiteral("voice-cloning")) {
            legacyConfig.familyId = settings->selectedVoiceCloneFamily();
            legacyConfig.runtimeId = settings->selectedTtsRuntime(); // Uses TTS runtime as legacy fallback
            legacyConfig.runtimeVersion = settings->selectedTtsRuntimeVersion();
        }

        legacyConfig.familyId = normalizeFamilyId(legacyConfig.familyId);
        legacyConfig.runtimeId = normalizeRuntimeId(legacyConfig.runtimeId);

        if (legacyConfig.isValid()) {
            if (!isCatalogSelection(legacyConfig)) {
                Logger::warning(QStringLiteral("StudioSelectionRepository"),
                                QStringLiteral("Skipping stale legacy selection for capability: %1 (Family: %2, Runtime: %3)")
                                    .arg(cap, legacyConfig.familyId, legacyConfig.runtimeId));
                continue;
            }
            Logger::info(QStringLiteral("StudioSelectionRepository"),
                         QStringLiteral("Migrating legacy selection for capability: %1 (Family: %2, Runtime: %3)").arg(cap, legacyConfig.familyId, legacyConfig.runtimeId));
            saveActiveSelection(legacyConfig);
        }
    }
}

} // namespace LAStudio
