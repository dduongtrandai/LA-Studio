#include "RuntimeManager.h"
#include "PathUtils.h"
#include "Logger.h"
#include <runtimes/WhisperInterface.h>
#include <runtimes/CrispKokoroInterface.h>
#include <runtimes/KokoroVietnameseInterface.h>
#include <runtimes/CrispQwen3TtsInterface.h>
#include <runtimes/VibevoiceInterface.h>
#include "core/CatalogManager.h"
#include "core/Settings.h"
#include "controllers/AppController.h"
#include "controllers/ModelSessionRegistry.h"

#include <QThreadPool>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVersionNumber>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <algorithm>

namespace LAStudio {

// ---------------------------------------------------------------------------
// Helper: derive engineFamily from a runtime id
// e.g. "crispasr-win-x86_64-cpu"   -> "crispasr"
//      "omnivoice.cpp-win-x86_64-cuda-12" -> "omnivoice.cpp"
// ---------------------------------------------------------------------------
static QString deriveEngineFamily(const QString &id)
{
    for (const auto &plat : {QStringLiteral("-win-"), QStringLiteral("-linux-"), QStringLiteral("-macos-")}) {
        int idx = id.indexOf(plat);
        if (idx > 0) return id.left(idx);
    }
    return id;
}

// ---------------------------------------------------------------------------
// Helper: derive variant from id given engineFamily
// e.g. id="crispasr-win-x86_64-cpu", family="crispasr" -> "win-x86_64-cpu"
// ---------------------------------------------------------------------------
static QString deriveVariant(const QString &id, const QString &engineFamily)
{
    if (id.startsWith(engineFamily + QStringLiteral("-"))) {
        return id.mid(engineFamily.length() + 1);
    }
    return id;
}

static QString normalizedDirectoryPath(const QString &path)
{
    QString normalized = QDir(path).canonicalPath();
    if (normalized.isEmpty()) {
        normalized = QDir(path).absolutePath();
    }
    normalized = QDir::cleanPath(normalized);
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

static bool isDirectoryInsideDirectory(const QString &directory, const QString &parentDirectory)
{
    const QString normalizedDirectory = normalizedDirectoryPath(directory);
    const QString normalizedParent = normalizedDirectoryPath(parentDirectory);
    if (normalizedDirectory.isEmpty() || normalizedParent.isEmpty() || normalizedDirectory == normalizedParent) {
        return false;
    }

    const QString parentRoot = normalizedParent.endsWith(u'/')
        ? normalizedParent
        : normalizedParent + u'/';
    return normalizedDirectory.startsWith(parentRoot);
}

static QString safeRuntimeFilePath(const QDir &runtimeDir, const QString &relativePath)
{
    if (relativePath.trimmed().isEmpty() || QDir::isAbsolutePath(relativePath)) {
        return {};
    }

    const QString cleanRelative = QDir::cleanPath(relativePath);
    if (cleanRelative == QStringLiteral("..") || cleanRelative.startsWith(QStringLiteral("../")) ||
        cleanRelative.startsWith(QStringLiteral("..\\"))) {
        return {};
    }

    const QString candidate = runtimeDir.absoluteFilePath(cleanRelative);
    if (!isDirectoryInsideDirectory(QFileInfo(candidate).absolutePath(), runtimeDir.absolutePath()) &&
        QFileInfo(candidate).absolutePath() != runtimeDir.absolutePath()) {
        return {};
    }
    return candidate;
}

// ---------------------------------------------------------------------------
// Migrate legacy flat runtime folders into hierarchical structure.
// Legacy: backends/crispasr-win-x86_64-cpu-v0.6.8/
// New:    backends/crispasr/win-x86_64-cpu-v0.6.8/
// ---------------------------------------------------------------------------
static void migrateLegacyFolders(const QString &backendsPath)
{
    QDir backendsDir(backendsPath);
    QStringList subdirs = backendsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const auto &subdir : subdirs) {
        QDir dir(backendsDir.absoluteFilePath(subdir));

        // Only migrate folders that have a backend-manifest.json directly
        // (i.e. legacy flat layout). New hierarchical family folders won't have one.
        QFile manifestFile(dir.absoluteFilePath(QStringLiteral("backend-manifest.json")));
        if (!manifestFile.exists()) continue;

        // Read manifest to get id + version
        if (!manifestFile.open(QIODevice::ReadOnly)) continue;
        auto doc = QJsonDocument::fromJson(manifestFile.readAll());
        manifestFile.close();
        if (!doc.isObject()) continue;

        QJsonObject obj = doc.object();
        QString id = obj.value(QStringLiteral("id")).toString();
        QString version = obj.value(QStringLiteral("version")).toString();
        if (id.isEmpty()) continue;

        // Derive engine family
        QString engineFamily = obj.value(QStringLiteral("engineFamily")).toString();
        if (engineFamily.isEmpty()) {
            engineFamily = deriveEngineFamily(id);
        }
        QString variant = deriveVariant(id, engineFamily);

        // Skip if the folder name IS the engine family name (already a family dir)
        if (subdir == engineFamily) continue;

        // Create family directory
        QString familyPath = backendsDir.absoluteFilePath(engineFamily);
        QDir().mkpath(familyPath);

        // Determine new folder name: {variant}-{version}
        QString newFolderName = variant;
        if (!version.isEmpty()) {
            newFolderName += QStringLiteral("-") + version;
        }

        QString destPath = familyPath + QDir::separator() + newFolderName;

        if (QDir(destPath).exists()) {
            // Already migrated or duplicate
            continue;
        }

        // Move the folder
        bool ok = QDir().rename(dir.absolutePath(), destPath);
        if (ok) {
            // Update the manifest with new fields
            QFile newManifest(destPath + QStringLiteral("/backend-manifest.json"));
            if (newManifest.open(QIODevice::ReadOnly)) {
                auto doc2 = QJsonDocument::fromJson(newManifest.readAll());
                newManifest.close();
                QJsonObject obj2 = doc2.object();
                obj2[QStringLiteral("engineFamily")] = engineFamily;
                obj2[QStringLiteral("variant")] = variant;
                if (newManifest.open(QIODevice::WriteOnly)) {
                    newManifest.write(QJsonDocument(obj2).toJson(QJsonDocument::Indented));
                    newManifest.close();
                }
            }
            Logger::info(QStringLiteral("RuntimeManager"),
                         QStringLiteral("Migrated legacy runtime: %1 -> %2/%3").arg(subdir, engineFamily, newFolderName));
        } else {
            Logger::error(QStringLiteral("RuntimeManager"),
                          QStringLiteral("Failed to migrate legacy runtime folder: %1").arg(subdir));
        }
    }
}

RuntimeManager::RuntimeManager(CatalogManager *catalog, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_settings(settings)
{
    scanRuntimes();
}

void RuntimeManager::scanRuntimes()
{
    // Fetch catalog-driven runtime candidates on the main thread (thread-safe copy)
    QVariantList catalogFamilies;
    if (m_catalog) {
        if (QThread::currentThread() == m_catalog->thread()) {
            catalogFamilies = m_catalog->ttsFamilies();
            catalogFamilies.append(m_catalog->sttFamilies());
        } else {
            QMetaObject::invokeMethod(m_catalog, [&catalogFamilies, this]() {
                catalogFamilies = m_catalog->ttsFamilies();
                catalogFamilies.append(m_catalog->sttFamilies());
            }, Qt::BlockingQueuedConnection);
        }
    }

    // Build a candidate filename list from the catalog (explicit library entries and generated patterns)
    QStringList catalogCandidates;
    QStringList exts = { QStringLiteral(".dll"), QStringLiteral(".so"), QStringLiteral(".dylib") };
    for (const QVariant &f : catalogFamilies) {
        QVariantMap fm = f.toMap();
        if (!fm.contains("runtimes")) continue;
        QVariantList runtimes = fm.value("runtimes").toList();
        for (const QVariant &r : runtimes) {
            QVariantMap rm = r.toMap();
            QString lib = rm.value("library").toString();
            if (!lib.isEmpty()) catalogCandidates << lib;
            QString id = rm.value("id").toString();
            if (!id.isEmpty()) {
                for (const QString &e : exts) {
                    catalogCandidates << id + e;
                    catalogCandidates << QStringLiteral("lib") + id + e;
                    catalogCandidates << QStringLiteral("bin/") + id + e;
                    catalogCandidates << QStringLiteral("lib/") + id + e;
                }
            }
        }
    }
    catalogCandidates << QStringLiteral("omnivoice.dll")
                      << QStringLiteral("bin/omnivoice.dll")
                      << QStringLiteral("vibevoice.dll")
                      << QStringLiteral("bin/vibevoice.dll")
                      << QStringLiteral("kokoro-vietnamese.dll")
                      << QStringLiteral("bin/kokoro-vietnamese.dll")
                      << QStringLiteral("crispasr.dll")
                      << QStringLiteral("bin/crispasr.dll");
    catalogCandidates.removeDuplicates();

    QPointer<RuntimeManager> weakThis(this);
    QThreadPool::globalInstance()->start([weakThis, catalogCandidates, catalogFamilies]() {
        QString backendsPath = PathUtils::backendsDir();
        QDir backendsDir(backendsPath);

        if (!backendsDir.exists()) {
            backendsDir.mkpath(".");
            QCoreApplication* app = QCoreApplication::instance();
            if (app && weakThis) {
                QMetaObject::invokeMethod(app, [weakThis]() {
                    if (!weakThis) return;
                    weakThis->m_runtimes.clear();
                    emit weakThis->registryUpdated();
                });
            }
            return;
        }

        // Phase 1: Auto-migrate legacy flat folders into hierarchical structure
        migrateLegacyFolders(backendsPath);

        // Phase 2: Scan hierarchical structure
        QVector<RuntimeInfo> results;

        QStringList topDirs = backendsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &topDir : topDirs) {
            QDir dir(backendsDir.absoluteFilePath(topDir));

            // Check if this is a legacy flat folder (has backend-manifest.json directly).
            // This handles the case where migration failed or wasn't possible.
            if (QFile::exists(dir.absoluteFilePath(QStringLiteral("backend-manifest.json")))) {
                RuntimeInfo info = weakThis ? weakThis->processRuntimeDir(dir, QString(), catalogCandidates, catalogFamilies) : RuntimeInfo();
                if (!info.id.isEmpty() && info.isUsable()) {
                    results.append(info);
                }
                continue;
            }

            // Treat as engine family folder — scan its subfolders
            QStringList variantDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto &variantDir : variantDirs) {
                QDir subDir(dir.absoluteFilePath(variantDir));
                RuntimeInfo info = weakThis ? weakThis->processRuntimeDir(subDir, topDir, catalogCandidates, catalogFamilies) : RuntimeInfo();
                if (!info.id.isEmpty() && info.isUsable()) {
                    results.append(info);
                }
            }
        }

        std::sort(results.begin(), results.end(), [](const RuntimeInfo &a, const RuntimeInfo &b) {
            if (a.id != b.id) return a.id < b.id;
            const QVersionNumber av = QVersionNumber::fromString(a.version);
            const QVersionNumber bv = QVersionNumber::fromString(b.version);
            if (!av.isNull() && !bv.isNull()) {
                return QVersionNumber::compare(av, bv) > 0;
            }
            return a.version > b.version;
        });

        QCoreApplication* app = QCoreApplication::instance();
        if (app && weakThis) {
            QMetaObject::invokeMethod(app, [weakThis, results]() {
                if (!weakThis) return;
                weakThis->m_runtimes = results;
                emit weakThis->registryUpdated();
            });
        }
    });
}

RuntimeInfo RuntimeManager::processRuntimeDir(const QDir &dir,
                                              const QString &familyHint,
                                              const QStringList &catalogCandidates,
                                              const QVariantList &catalogFamilies) const
{
    RuntimeInfo info;
    QFile manifestFile(dir.absoluteFilePath(QStringLiteral("backend-manifest.json")));

    if (!manifestFile.open(QIODevice::ReadOnly)) {
        // No manifest — try fallback heuristics (same as old code)
        QString subdir = dir.dirName();
        if (!subdir.contains('-')) return info;

        int lastDash = subdir.lastIndexOf('-');
        info.id = subdir.left(lastDash);
        info.version = subdir.mid(lastDash + 1);
        info.name = info.id;
        info.type = QStringLiteral("stt");
        info.directory = dir.absolutePath();

        // Set engineFamily and variant
        if (!familyHint.isEmpty()) {
            info.engineFamily = familyHint;
            info.variant = deriveVariant(info.id, info.engineFamily);
        } else {
            info.engineFamily = deriveEngineFamily(info.id);
            info.variant = deriveVariant(info.id, info.engineFamily);
        }

        QStringList candidates = catalogCandidates;
        for (const auto &c : candidates) {
            if (dir.exists(c)) {
                info.libraryPath = dir.absoluteFilePath(c);
                if (c.contains(QStringLiteral("omnivoice")) ||
                    c.contains(QStringLiteral("vibevoice")) ||
                    c.contains(QStringLiteral("crispasr")) ||
                    c.contains(QStringLiteral("kokoro")) ||
                    c.contains(QStringLiteral("vieneu"))) {
                    info.type = QStringLiteral("tts");
                } else {
                    info.type = QStringLiteral("stt");
                }
                break;
            }
        }

        if (!info.libraryPath.isEmpty()) {
            // Try to prefer a friendly name from the app catalog
            for (const QVariant &f : catalogFamilies) {
                QVariantMap fm = f.toMap();
                if (!fm.contains("runtimes")) continue;
                QVariantList runtimes = fm.value("runtimes").toList();
                for (const QVariant &r : runtimes) {
                    QVariantMap rm = r.toMap();
                    if (rm.value("id").toString() == info.id) {
                        QString cname = rm.value("name").toString();
                        if (!cname.isEmpty()) info.name = cname;
                        break;
                    }
                }
                if (info.name != info.id) break;
            }
        }

        return info;
    }

    // Parse manifest
    auto data = manifestFile.readAll();
    manifestFile.close(); // Close file handle immediately to allow subsequent writes on Windows
    auto doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        Logger::error("RuntimeManager", "Failed to parse manifest in " + dir.dirName());
        return info;
    }

    QJsonObject obj = doc.object();
    info.id = obj.value("id").toString();
    info.name = obj.value("name").toString();
    info.version = obj.value("version").toString();
    info.type = obj.value("type").toString();
    info.directory = dir.absolutePath();
    info.kind = obj.value(QStringLiteral("kind")).toString(QStringLiteral("dynamic-library"));
    info.protocolVersion = obj.value(QStringLiteral("protocolVersion")).toString();
    const QJsonArray capabilities = obj.value(QStringLiteral("capabilities")).toArray();
    for (const QJsonValue &value : capabilities) info.capabilities.append(value.toString());
    const QJsonArray modelFormats = obj.value(QStringLiteral("modelFormats")).toArray();
    for (const QJsonValue &value : modelFormats) info.modelFormats.append(value.toString());

    // Read engineFamily/variant from manifest, or derive them
    info.engineFamily = obj.value("engineFamily").toString();
    info.variant = obj.value("variant").toString();
    if (info.engineFamily.isEmpty()) {
        info.engineFamily = !familyHint.isEmpty() ? familyHint : deriveEngineFamily(info.id);
    }
    if (info.variant.isEmpty()) {
        info.variant = deriveVariant(info.id, info.engineFamily);
    }

    // Self-healing: if the parsed engine family does not match the actual folder familyHint, correction is needed
    bool manifestChanged = false;
    if (!familyHint.isEmpty() && info.engineFamily != familyHint) {
        info.engineFamily = familyHint;
        manifestChanged = true;
    }

    // Ensure the ID conforms to: {engineFamily}-{variant}
    QString expectedId = info.engineFamily + QStringLiteral("-") + info.variant;
    if (info.id != expectedId) {
        info.id = expectedId;
        manifestChanged = true;
    }

    if (manifestChanged) {
        obj[QStringLiteral("engineFamily")] = info.engineFamily;
        obj[QStringLiteral("id")] = info.id;

        // Save corrected manifest file back to disk
        QFile outFile(manifestFile.fileName());
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
            outFile.close();
            Logger::info("RuntimeManager", "Corrected and saved manifest file on-the-fly for ID: " + info.id);
        }
    }

    const QString libName = obj.value("library").toString();
    const QString entrypoint = obj.value(QStringLiteral("entrypoint")).toString();
    if (info.kind == QStringLiteral("process")) {
        info.executablePath = safeRuntimeFilePath(dir, entrypoint);
        if (!QFileInfo(info.executablePath).isFile()) {
            Logger::error(QStringLiteral("RuntimeManager"),
                          QStringLiteral("Skipping process runtime with missing or unsafe entrypoint: %1").arg(dir.dirName()));
            return RuntimeInfo();
        }
    } else if (libName.isEmpty()) {
        QStringList candidates = catalogCandidates;
        for (const auto &c : candidates) {
            if (dir.exists(c)) {
                info.libraryPath = dir.absoluteFilePath(c);
                break;
            }
        }
    } else {
        info.libraryPath = safeRuntimeFilePath(dir, libName);
    }

    if (info.type.isEmpty() && !info.libraryPath.isEmpty()) {
        if (info.libraryPath.contains(QStringLiteral("omnivoice")) ||
            info.libraryPath.contains(QStringLiteral("vibevoice")) ||
            info.libraryPath.contains(QStringLiteral("crispasr")) ||
            info.libraryPath.contains(QStringLiteral("kokoro")) ||
            info.libraryPath.contains(QStringLiteral("vieneu"))) {
            info.type = QStringLiteral("tts");
        } else {
            info.type = QStringLiteral("stt");
        }
    }

    info.metadata = obj.value("metadata").toVariant().toMap();

    // If manifest didn't provide a friendly name, try the catalog
    if (info.name.isEmpty()) {
        for (const QVariant &f : catalogFamilies) {
            QVariantMap fm = f.toMap();
            if (!fm.contains("runtimes")) continue;
            QVariantList runtimes = fm.value("runtimes").toList();
            for (const QVariant &r : runtimes) {
                QVariantMap rm = r.toMap();
                if (rm.value("id").toString() == info.id) {
                    QString cname = rm.value("name").toString();
                    if (!cname.isEmpty()) info.name = cname;
                    break;
                }
            }
            if (!info.name.isEmpty()) break;
        }
    }

    if (info.id.isEmpty()) {
        return RuntimeInfo();
    }
    if (!info.isUsable()) {
        Logger::error("RuntimeManager", "Skipping runtime with no valid entrypoint: " + dir.dirName());
        return RuntimeInfo();
    }

    return info;
}

RuntimeInfo RuntimeManager::inspectRuntimeDirectory(const QString &directory) const
{
    return processRuntimeDir(QDir(directory), QString(), {}, {});
}

bool RuntimeManager::loadRuntime(const QString &id)
{
    for (const auto &info : m_runtimes) {
        if (info.id == id) {
            return WhisperInterface::instance().load(info.libraryPath);
        }
    }
    return false;
}

bool RuntimeManager::loadTtsRuntime(const QString &id)
{
    for (const auto &info : m_runtimes) {
        if (info.id == id) {
            if (info.libraryPath.contains(QStringLiteral("kokoro-vietnamese"), Qt::CaseInsensitive)) {
                return KokoroVietnameseInterface::instance().load(info.libraryPath);
            }
            // Prefer a dedicated Vibevoice interface when the library looks like vibevoice
            if (info.libraryPath.contains(QStringLiteral("vibevoice"), Qt::CaseInsensitive)) {
                return VibevoiceInterface::instance().load(info.libraryPath);
            }
            if (info.libraryPath.contains(QStringLiteral("qwen3"), Qt::CaseInsensitive)) {
                return CrispQwen3TtsInterface::instance().load(info.libraryPath);
            }
            if (info.libraryPath.contains(QStringLiteral("vieneu"), Qt::CaseInsensitive)) {
                return VieneuTtsInterface::instance().load(info.libraryPath);
            }
            if (info.libraryPath.contains(QStringLiteral("crispasr"), Qt::CaseInsensitive) ||
                info.libraryPath.contains(QStringLiteral("kokoro"), Qt::CaseInsensitive)) {
                return CrispKokoroInterface::instance().load(info.libraryPath);
            }
            return OmnivoiceInterface::instance().load(info.libraryPath);
        }
    }
    return false;
}

QString RuntimeManager::getRuntimePath(const QString &id) const
{
    for (const auto &info : m_runtimes) {
        if (info.id == id) return info.libraryPath;
    }
    return QString();
}

QString RuntimeManager::getRuntimePathForVersion(const QString &id, const QString &version) const
{
    if (version.isEmpty()) return getRuntimePath(id);

    for (const auto &info : m_runtimes) {
        if (info.id == id && info.version == version) return info.libraryPath;
    }
    return QString();
}

QString RuntimeManager::getRuntimeExecutablePath(const QString &id) const
{
    for (const auto &info : m_runtimes) {
        if (info.id == id && info.kind == QStringLiteral("process")) return info.executablePath;
    }
    return {};
}

QString RuntimeManager::getRuntimeExecutablePathForVersion(const QString &id, const QString &version) const
{
    if (version.isEmpty()) return getRuntimeExecutablePath(id);
    for (const auto &info : m_runtimes) {
        if (info.id == id && info.version == version && info.kind == QStringLiteral("process")) {
            return info.executablePath;
        }
    }
    return {};
}

QString RuntimeManager::getRuntimeKindForVersion(const QString &id, const QString &version) const
{
    for (const auto &info : m_runtimes) {
        if (info.id == id && (version.isEmpty() || info.version == version)) return info.kind;
    }
    return {};
}

QVariantList RuntimeManager::runtimeVersions(const QString &id) const
{
    QVariantList list;
    for (const auto &info : m_runtimes) {
        if (info.id != id) continue;

        QVariantMap m;
        m["id"] = info.id;
        m["engineFamily"] = info.engineFamily;
        m["variant"] = info.variant;
        m["name"] = info.name;
        m["version"] = info.version;
        m["type"] = info.type;
        m["directory"] = info.directory;
        m["libraryPath"] = info.libraryPath;
        m["kind"] = info.kind;
        m["executablePath"] = info.executablePath;
        m["protocolVersion"] = info.protocolVersion;
        m["capabilities"] = info.capabilities;
        m["modelFormats"] = info.modelFormats;
        m["metadata"] = info.metadata;
        list.append(m);
    }
    return list;
}

bool RuntimeManager::removeRuntimeVersion(const QString &id, const QString &version)
{
    AppController *app = AppController::instance();
    ModelSessionRegistry *sessionRegistry = app ? app->sessionRegistry() : nullptr;

    for (const auto &info : m_runtimes) {
        if (info.id != id || info.version != version) continue;

        if (sessionRegistry) {
            const ResourceReleaseResult result = sessionRegistry->prepareRuntimeRemoval(id, version);
            if (result == ResourceReleaseResult::BusyProcessing) {
                Logger::warning("RuntimeManager",
                                QStringLiteral("Cannot remove runtime while session is processing: %1 %2")
                                    .arg(id, version));
                return false;
            }
            if (result == ResourceReleaseResult::Pending) {
                IModelSession *sttSession = sessionRegistry->sessionForCapability(QStringLiteral("stt"));
                IModelSession *ttsSession = sessionRegistry->sessionForCapability(QStringLiteral("tts"));
                auto stillUsingRuntime = [sttSession, ttsSession, id, version]() {
                    const QList<IModelSession*> sessions = { sttSession, ttsSession };
                    for (IModelSession *session : sessions) {
                        if (session && session->usesRuntime(id, version)) {
                            return true;
                        }
                    }
                    return false;
                };

                if (stillUsingRuntime()) {
                    QEventLoop loop;
                    QTimer timeout;
                    timeout.setSingleShot(true);
                    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
                    if (sttSession) {
                        QObject::connect(sttSession, &IModelSession::stateChanged, &loop, [&]() {
                            if (!stillUsingRuntime()) loop.quit();
                        });
                    }
                    if (ttsSession) {
                        QObject::connect(ttsSession, &IModelSession::stateChanged, &loop, [&]() {
                            if (!stillUsingRuntime()) loop.quit();
                        });
                    }
                    timeout.start(5000);
                    loop.exec();
                }

                if (stillUsingRuntime()) {
                    Logger::warning("RuntimeManager",
                                    QStringLiteral("Runtime is still in use after unload wait window: %1 %2")
                                        .arg(id, version));
                    return false;
                }
            }
        }

        // Unload dynamically loaded engines if we are removing the active version
        emit runtimeVersionAboutToBeRemoved(id, version);

        if (!isDirectoryInsideDirectory(info.directory, PathUtils::backendsDir())) {
            Logger::error("RuntimeManager", "Refusing to remove runtime outside backends path: " + info.directory);
            return false;
        }

        QDir dir(info.directory);
        const bool ok = dir.removeRecursively();

        // Clean up empty engine family folder
        if (ok && !info.engineFamily.isEmpty()) {
            QDir familyDir(PathUtils::backendsDir() + QStringLiteral("/") + info.engineFamily);
            if (familyDir.exists() && familyDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                familyDir.removeRecursively();
            }
        }

        if (ok) scanRuntimes();
        return ok;
    }
    return false;
}

QString RuntimeManager::backendsPath() const
{
    return PathUtils::backendsDir();
}

QVariantList RuntimeManager::allRuntimes() const
{
    QVariantList list;
    for (const auto &info : m_runtimes) {
        QVariantMap m;
        m["id"] = info.id;
        m["engineFamily"] = info.engineFamily;
        m["variant"] = info.variant;
        m["name"] = info.name;
        m["version"] = info.version;
        m["type"] = info.type;
        m["directory"] = info.directory;
        m["libraryPath"] = info.libraryPath;
        m["kind"] = info.kind;
        m["executablePath"] = info.executablePath;
        m["protocolVersion"] = info.protocolVersion;
        m["capabilities"] = info.capabilities;
        m["modelFormats"] = info.modelFormats;
        m["metadata"] = info.metadata;
        list.append(m);
    }
    return list;
}

} // namespace LAStudio
