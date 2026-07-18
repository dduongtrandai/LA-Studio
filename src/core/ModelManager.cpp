#include "ModelManager.h"
#include "PathUtils.h"
#include "Logger.h"
#include "controllers/app/AppController.h"
#include "controllers/models/ModelSessionRegistry.h"
#include <QThreadPool>

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QRegularExpression>
#include <QDateTime>
#include <QTextStream>
#include <functional>

namespace LAStudio {

namespace {

QString normalizedModelIdPath(const QString &modelId)
{
    QString path = modelId.trimmed();
    path.replace(QStringLiteral("\\"), QStringLiteral("/"));
    while (path.startsWith(QStringLiteral("/"))) {
        path.remove(0, 1);
    }
    while (path.endsWith(QStringLiteral("/"))) {
        path.chop(1);
    }
    return path;
}

QString repoTailWithoutGguf(const QString &modelId)
{
    QString repoTail = modelId.contains(QStringLiteral("/")) ? modelId.section(QStringLiteral("/"), -1) : modelId;
    if (repoTail.endsWith(QStringLiteral("-GGUF"), Qt::CaseInsensitive)) {
        repoTail.chop(5);
    }
    return repoTail;
}

QStringList modelPathCandidates(const QString &modelId)
{
    const QString normalized = normalizedModelIdPath(modelId);
    const QString altId = QString(modelId).replace(QStringLiteral("/"), QStringLiteral("_"));
    const QString repoTail = modelId.contains(QStringLiteral("/")) ? modelId.section(QStringLiteral("/"), -1) : modelId;
    const QString repoNoGguf = repoTailWithoutGguf(modelId);

    QStringList candidates;
    if (!normalized.isEmpty()) {
        candidates << normalized;
        candidates << QStringLiteral("models/") + normalized;
    }
    if (!altId.isEmpty()) candidates << altId;
    if (!repoTail.isEmpty()) {
        candidates << repoTail;
        candidates << QStringLiteral("models/") + repoTail;
    }
    if (!repoNoGguf.isEmpty()) {
        candidates << repoNoGguf;
        candidates << QStringLiteral("models/") + repoNoGguf;
    }
    candidates.removeDuplicates();
    return candidates;
}

void scanConcreteModelTree(const QDir &concreteRoot,
                           const std::function<void(const QDir &, const QString &)> &scanModelDir)
{
    if (!concreteRoot.exists()) {
        return;
    }

    const QStringList publisherDirs = concreteRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &publisher : publisherDirs) {
        QDir publisherDir(concreteRoot.absoluteFilePath(publisher));
        if (!publisherDir.entryList(QDir::Files).isEmpty()) {
            scanModelDir(publisherDir, publisher);
        }

        const QStringList repoDirs = publisherDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &repo : repoDirs) {
            scanModelDir(QDir(publisherDir.absoluteFilePath(repo)),
                         publisher + QStringLiteral("/") + repo);
        }
    }
}

void parseYamlMetadata(const QString &yamlPath, QStringList &archs, QStringList &params)
{
    QFile file(yamlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream in(&file);
    bool inArch = false;
    bool inParams = false;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith(QStringLiteral("architectures:"))) {
            inArch = true;
            inParams = false;
            continue;
        }
        if (line.startsWith(QStringLiteral("paramsStrings:")) || line.startsWith(QStringLiteral("params:"))) {
            inParams = true;
            inArch = false;
            continue;
        }
        if (line.endsWith(QStringLiteral(":")) && !line.startsWith(QStringLiteral("-"))) {
            inArch = false;
            inParams = false;
        }
        if (line.startsWith(QStringLiteral("-"))) {
            QString val = line.mid(1).trimmed();
            if (val.startsWith(QStringLiteral("\"")) && val.endsWith(QStringLiteral("\""))) {
                val = val.mid(1, val.length() - 2);
            } else if (val.startsWith(QStringLiteral("'")) && val.endsWith(QStringLiteral("'"))) {
                val = val.mid(1, val.length() - 2);
            }
            if (inArch && !val.isEmpty()) {
                archs.append(val);
            } else if (inParams && !val.isEmpty()) {
                params.append(val);
            }
        }
    }
}

QString extractQuantFromFilename(const QString &filename)
{
    static const QRegularExpression regex(QStringLiteral("(?:^|[-_\\.])((?:i)?q\\d_[a-z0-9_]+|f16|f32|bf16|q\\d_[\\d\\w]+|q\\d_\\w)(?:\\.|-|$)"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regex.match(filename);
    if (match.hasMatch()) {
        return match.captured(1).toUpper();
    }
    QString lower = filename.toLower();
    if (lower.contains(QStringLiteral("q4_k_m"))) return QStringLiteral("Q4_K_M");
    if (lower.contains(QStringLiteral("q4_k_s"))) return QStringLiteral("Q4_K_S");
    if (lower.contains(QStringLiteral("q8_0"))) return QStringLiteral("Q8_0");
    if (lower.contains(QStringLiteral("q4_0"))) return QStringLiteral("Q4_0");
    if (lower.contains(QStringLiteral("q2_k"))) return QStringLiteral("Q2_K");
    if (lower.contains(QStringLiteral("q3_k_l"))) return QStringLiteral("Q3_K_L");
    if (lower.contains(QStringLiteral("q3_k_m"))) return QStringLiteral("Q3_K_M");
    if (lower.contains(QStringLiteral("q5_k_m"))) return QStringLiteral("Q5_K_M");
    if (lower.contains(QStringLiteral("q5_0"))) return QStringLiteral("Q5_0");
    if (lower.contains(QStringLiteral("q6_k"))) return QStringLiteral("Q6_K");
    if (lower.contains(QStringLiteral("f16"))) return QStringLiteral("F16");
    if (lower.contains(QStringLiteral("f32"))) return QStringLiteral("F32");
    if (lower.contains(QStringLiteral("bf16"))) return QStringLiteral("BF16");
    return QStringLiteral("Unknown");
}

QString formatRelativeTime(const QDateTime &dateTime)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 secs = dateTime.secsTo(now);
    if (secs < 60) return QStringLiteral("Just now");
    qint64 mins = secs / 60;
    if (mins < 60) return QString::number(mins) + QStringLiteral(" minutes ago");
    qint64 hours = mins / 60;
    if (hours < 24) return QString::number(hours) + QStringLiteral(" hours ago");
    qint64 days = hours / 24;
    return QString::number(days) + QStringLiteral(" days ago");
}

QString guessArch(const QString &modelId)
{
    QString lower = modelId.toLower();
    if (lower.contains(QStringLiteral("gemma"))) return QStringLiteral("gemma");
    if (lower.contains(QStringLiteral("qwen"))) return QStringLiteral("qwen");
    if (lower.contains(QStringLiteral("llama"))) return QStringLiteral("llama");
    if (lower.contains(QStringLiteral("granite"))) return QStringLiteral("granite");
    if (lower.contains(QStringLiteral("openelm"))) return QStringLiteral("openelm");
    if (lower.contains(QStringLiteral("kokoro"))) return QStringLiteral("styletts2");
    if (lower.contains(QStringLiteral("omnivoice"))) return QStringLiteral("omnivoice");
    if (lower.contains(QStringLiteral("voxcpm"))) return QStringLiteral("voxcpm");
    return QStringLiteral("unknown");
}

QString guessParams(const QString &modelId)
{
    QString lower = modelId.toLower();
    static const QRegularExpression regex(QStringLiteral("(\\d+(?:\\.\\d+)?(?:b|m))"), QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = regex.match(lower);
    if (match.hasMatch()) {
        return match.captured(1).toUpper();
    }
    return QStringLiteral("unknown");
}

}

ModelManager::ModelManager(QObject *parent)
    : QAbstractListModel(parent)
{
    QSettings settings;
    m_modelsRoot = QDir(settings.value(QStringLiteral("storage/modelsPath"), PathUtils::modelsDir()).toString()).absolutePath();
    Logger::info(QStringLiteral("ModelManager"), QStringLiteral("Using models root: %1").arg(m_modelsRoot));
    scanLocalModels();
}

int ModelManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_models.size();
}

QVariant ModelManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_models.size())
        return QVariant();

    const ModelInfo &model = m_models[index.row()];

    switch (role) {
    case IdRole: return model.id;
    case TaskRole: return model.task;
    case FormatRole: return model.format;
    case PathRole: return model.path;
    case FilesRole: return model.files;
    case SizeRole: return model.size;
    case TotalSizeRole: return model.totalSize;
    case ArchRole: return model.arch;
    case ParamsRole: return model.params;
    case PublisherRole: return model.publisher;
    case QuantRole: return model.quant;
    case ModifiedRole: return model.modified;
    default: return QVariant();
    }
}

QHash<int, QByteArray> ModelManager::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TaskRole] = "task";
    roles[FormatRole] = "format";
    roles[PathRole] = "path";
    roles[FilesRole] = "files";
    roles[SizeRole] = "size";
    roles[TotalSizeRole] = "totalSize";
    roles[ArchRole] = "arch";
    roles[ParamsRole] = "params";
    roles[PublisherRole] = "publisher";
    roles[QuantRole] = "quant";
    roles[ModifiedRole] = "modified";
    return roles;
}

QString ModelManager::modelsRoot() const
{
    return m_modelsRoot;
}

void ModelManager::setModelsRoot(const QString &root)
{
    const QString normalizedRoot = QDir(root).absolutePath();
    if (m_modelsRoot != normalizedRoot) {
        m_modelsRoot = normalizedRoot;
        Logger::info(QStringLiteral("ModelManager"), QStringLiteral("Models root changed to: %1").arg(m_modelsRoot));
        emit modelsRootChanged();
        // The caller (AppController) should call scanLocalModels() after this.
    }
}

void ModelManager::scanLocalModels()
{
    Logger::info(QStringLiteral("ModelManager"), QStringLiteral("Scanning local models from: %1").arg(m_modelsRoot));
    QVector<ModelInfo> scannedModels;

    struct VirtualMetadata {
        QString virtualId;
        QString arch;
        QString params;
    };
    QHash<QString, VirtualMetadata> virtualMetadataMap;

    QDir hubDir(PathUtils::hubModelsDir());
    if (hubDir.exists()) {
        QStringList publishers = hubDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &pub : publishers) {
            QDir pubDir(hubDir.absoluteFilePath(pub));
            QStringList repos = pubDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &repo : repos) {
                QDir repoDir(pubDir.absoluteFilePath(repo));
                QFile manifestFile(repoDir.absoluteFilePath(QStringLiteral("manifest.json")));
                if (manifestFile.open(QIODevice::ReadOnly)) {
                    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
                    QJsonObject obj = doc.object();
                    QString virtualId = obj.value(QStringLiteral("id")).toString();
                    if (!virtualId.isEmpty()) {
                        QString yamlPath = repoDir.absoluteFilePath(QStringLiteral("model.yaml"));
                        QStringList archs, params;
                        parseYamlMetadata(yamlPath, archs, params);
                        
                        QString arch = archs.isEmpty() ? QString() : archs.first();
                        QString paramStr = params.isEmpty() ? QString() : params.first();
                        
                        VirtualMetadata vm = { virtualId, arch, paramStr };
                        
                        QJsonArray requirements = obj.value(QStringLiteral("requirements")).toArray();
                        for (const QJsonValue &reqVal : requirements) {
                            QString reqId = reqVal.toString();
                            if (!reqId.isEmpty()) {
                                virtualMetadataMap.insert(reqId, vm);
                            }
                        }
                        QJsonArray downloadSources = obj.value(QStringLiteral("downloadSources")).toArray();
                        for (const QJsonValue &dsVal : downloadSources) {
                            QString dsId = dsVal.toString();
                            if (!dsId.isEmpty()) {
                                virtualMetadataMap.insert(dsId, vm);
                            }
                        }
                    }
                    manifestFile.close();
                }
            }
        }
    }

    auto appendModel = [&scannedModels](const QString &id, const QString &task, const QString &format,
                                        const QString &path, const QStringList &filenames, const QString &size,
                                        const QString &arch, const QString &params, const QString &publisher,
                                        const QString &quant, const QString &modified) {
        for (ModelInfo &model : scannedModels) {
            if (model.id == id) {
                if (model.path != path) {
                    model.path = path;
                }
                for (const QString &f : filenames) {
                    if (!model.files.contains(f)) {
                        model.files.append(f);
                    }
                }
                return;
            }
        }

        ModelInfo info;
        info.id = id;
        info.author = QStringLiteral("local");
        info.task = task;
        info.format = format;
        info.path = path;
        info.files = filenames;
        info.size = size;
        info.arch = arch;
        info.params = params;
        info.publisher = publisher;
        info.quant = quant;
        info.modified = modified;
        scannedModels.append(info);
    };

    auto scanModelDir = [&appendModel, &virtualMetadataMap](const QDir &subDir, const QString &defaultId) {
        QStringList files = subDir.entryList(QDir::Files);
        const QString subDirName = QFileInfo(subDir.absolutePath()).fileName();
        
        QString recoveredId = defaultId;
        QString recoveredTask = QStringLiteral("stt"); // Default

        // Try to load metadata if exists (.la-info.json preferred, with legacy .la-info fallback)
        QFile infoFile(subDir.absoluteFilePath(QStringLiteral(".la-info.json")));
        if (!infoFile.exists()) {
            infoFile.setFileName(subDir.absoluteFilePath(QStringLiteral(".la-info")));
        }
        if (infoFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(infoFile.readAll());
            QJsonObject info = doc.object();
            if (info.contains(QStringLiteral("id"))) recoveredId = info.value(QStringLiteral("id")).toString();
            if (info.contains(QStringLiteral("task"))) recoveredTask = info.value(QStringLiteral("task")).toString();
            infoFile.close();
        } else {
            // Legacy ID recovery logic
            if (subDirName.contains(QStringLiteral("_"))) {
                int firstUnderscore = subDirName.indexOf(QStringLiteral("_"));
                QString legacyRecoveredId = subDirName;
                legacyRecoveredId[firstUnderscore] = '/';
                recoveredId = legacyRecoveredId;
            }
        }

        QString finalModelId = recoveredId;
        if (finalModelId.isEmpty()) {
            finalModelId = defaultId;
        }
        if (finalModelId.isEmpty()) {
            finalModelId = subDirName;
        }

        // Map to virtual/catalog ID if present
        QString finalArch;
        QString finalParams;
        QString resolvedId = finalModelId;

        if (virtualMetadataMap.contains(finalModelId)) {
            VirtualMetadata vm = virtualMetadataMap.value(finalModelId);
            resolvedId = vm.virtualId;
            finalArch = vm.arch;
            finalParams = vm.params;
        } else if (virtualMetadataMap.contains(defaultId)) {
            VirtualMetadata vm = virtualMetadataMap.value(defaultId);
            resolvedId = vm.virtualId;
            finalArch = vm.arch;
            finalParams = vm.params;
        } else if (virtualMetadataMap.contains(subDirName)) {
            VirtualMetadata vm = virtualMetadataMap.value(subDirName);
            resolvedId = vm.virtualId;
            finalArch = vm.arch;
            finalParams = vm.params;
        }

        if (finalArch.isEmpty()) {
            finalArch = guessArch(resolvedId);
        }
        if (finalParams.isEmpty() || finalParams == QStringLiteral("unknown")) {
            finalParams = guessParams(resolvedId);
        }

        QString quantPublisher = QStringLiteral("local");
        if (defaultId.contains(QStringLiteral("/"))) {
            quantPublisher = defaultId.section(QStringLiteral("/"), 0, 0);
        }

        for (const QString &filename : files) {
            if (filename.contains(QStringLiteral("tts"), Qt::CaseInsensitive) ||
                filename.contains(QStringLiteral("kokoro"), Qt::CaseInsensitive) ||
                filename.contains(QStringLiteral("omnivoice"), Qt::CaseInsensitive) ||
                filename.contains(QStringLiteral("vibevoice"), Qt::CaseInsensitive) ||
                filename.contains(QStringLiteral("qwen3-tts"), Qt::CaseInsensitive)) {
                recoveredTask = QStringLiteral("tts");
                break;
            }
        }

        QString mainFilename;
        qint64 maxFileSize = -1;
        qint64 totalBytes = 0;
        QDateTime latestMod;
        QStringList modelFiles;
        
        for (const QString &filename : files) {
            if (filename == QStringLiteral(".la-info.json") || filename == QStringLiteral(".la-info") || filename == QStringLiteral("registry.json"))
                continue;

            QFileInfo fi(subDir.absoluteFilePath(filename));
            totalBytes += fi.size();
            QDateTime mod = fi.lastModified();
            if (!latestMod.isValid() || mod > latestMod) {
                latestMod = mod;
            }
            if (fi.size() > maxFileSize) {
                maxFileSize = fi.size();
                mainFilename = filename;
            }
            modelFiles.append(filename);
        }

        if (modelFiles.isEmpty()) {
            return;
        }

        QString quantVal = QStringLiteral("Unknown");
        if (!mainFilename.isEmpty()) {
            quantVal = extractQuantFromFilename(mainFilename);
        }

        QString sizeStr;
        if (totalBytes >= 1024 * 1024 * 1024) {
            sizeStr = QString::number(totalBytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
        } else {
            sizeStr = QString::number(totalBytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        }

        QString modifiedStr = latestMod.isValid() ? formatRelativeTime(latestMod) : QString();

        QString format = QStringLiteral("bin");
        if (mainFilename.endsWith(QStringLiteral(".gguf"))) format = QStringLiteral("gguf");
        else if (mainFilename.endsWith(QStringLiteral(".onnx"))) format = QStringLiteral("onnx");

        appendModel(resolvedId, recoveredTask, format, subDir.absolutePath(), modelFiles, sizeStr,
                    finalArch, finalParams, quantPublisher, quantVal, modifiedStr);
    };

    QDir rootDir(m_modelsRoot);

    // Current LA Studio concrete weights:
    // <modelsRoot>/<publisher>/<repo>/*
    scanConcreteModelTree(rootDir, scanModelDir);

    // Compatibility with the accidental nested layout created by older builds:
    // <modelsRoot>/models/<publisher>/<repo>/*
    scanConcreteModelTree(QDir(rootDir.absoluteFilePath(QStringLiteral("models"))), scanModelDir);

    // Legacy LA Studio layout: <modelsRoot>/<localDir>/*
    QStringList subDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subDirName : subDirs) {
        if (subDirName == QStringLiteral("models") ||
            subDirName == QStringLiteral("hub") ||
            subDirName == QStringLiteral("backends")) {
            continue;
        }
        QDir subDir(rootDir.absoluteFilePath(subDirName));
        scanModelDir(subDir, subDirName);
    }

    beginResetModel();
    const int oldCount = m_models.size();
    m_models = scannedModels;
    endResetModel();

    if (oldCount != m_models.size()) {
        emit countChanged();
    }
    
    m_version++;
    saveRegistry();
    emit registryUpdated();
}

void ModelManager::addModel(const QString &id, const QString &task, const QString &format,
                            const QString &path, const QStringList &files, const QString &size)
{
    bool changed = false;
    bool found = false;

    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models[i].id == id) {
            found = true;
            if (m_models[i].path != path) {
                m_models[i].path = path;
                changed = true;
            }
            for (const QString &f : files) {
                if (!m_models[i].files.contains(f)) {
                    m_models[i].files << f;
                    changed = true;
                }
            }
            break;
        }
    }

    if (!found) {
        beginInsertRows(QModelIndex(), m_models.size(), m_models.size());
        ModelInfo info;
        info.id = id;
        info.author = QStringLiteral("local");
        info.task = task;
        info.format = format;
        info.path = path;
        info.files = files;
        info.size = size;

        info.arch = guessArch(id);
        info.params = guessParams(id);

        QString quantPublisher = QStringLiteral("local");
        if (id.contains(QStringLiteral("/"))) {
            quantPublisher = id.section(QStringLiteral("/"), 0, 0);
        }
        info.publisher = quantPublisher;

        QString quantVal = QStringLiteral("Unknown");
        if (!files.isEmpty()) {
            quantVal = extractQuantFromFilename(files.first());
        }
        info.quant = quantVal;
        info.modified = formatRelativeTime(QDateTime::currentDateTime());

        m_models.append(info);
        endInsertRows();
        changed = true;
        emit countChanged();
    }

    if (found && changed) {
        emit dataChanged(index(0, 0), index(m_models.size() - 1, 0));
    }

    if (changed) {
        m_version++;
        emit registryUpdated();
        saveRegistry();
    }
}

void ModelManager::removeModel(const QString &id)
{
    QString altId = QString(id).replace("/", "_");
    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models[i].id == id || m_models[i].id == altId) {
            AppController *app = AppController::instance();
            ModelSessionRegistry *sessionRegistry = app ? app->sessionRegistry() : nullptr;
            if (sessionRegistry) {
                const ResourceReleaseResult result = sessionRegistry->prepareModelRemoval(m_models[i].path);
                if (result == ResourceReleaseResult::BusyProcessing ||
                    result == ResourceReleaseResult::Pending)
                {
                    Logger::warning(QStringLiteral("ModelManager"),
                                    QStringLiteral("Model is in use and cannot be removed now: %1")
                                    .arg(m_models[i].path));
                    return;
                }
            }

            beginRemoveRows(QModelIndex(), i, i);
             
            // Delete folder on disk
            QDir dir(m_models[i].path);
            dir.removeRecursively();
            
            m_models.removeAt(i);
            endRemoveRows();
            m_version++;
            emit countChanged();
            emit registryUpdated();
            saveRegistry();
            return;
        }
    }
}

bool ModelManager::hasFile(const QString &modelId, const QString &filename) const
{
    const QString altId = QString(modelId).replace(QStringLiteral("/"), QStringLiteral("_"));
    const QString repoTail = modelId.contains(QStringLiteral("/")) ? modelId.section(QStringLiteral("/"), -1) : modelId;
    const QString repoTailNoGguf = repoTailWithoutGguf(modelId);

    for (const auto &m : m_models) {
        const QString dirName = QFileInfo(m.path).fileName();
        const bool idMatch =
            (m.id == modelId) ||
            (m.id == altId) ||
            (m.id.compare(repoTail, Qt::CaseInsensitive) == 0) ||
            (m.id.compare(repoTailNoGguf, Qt::CaseInsensitive) == 0) ||
            (dirName.compare(repoTail, Qt::CaseInsensitive) == 0) ||
            (dirName.compare(repoTailNoGguf, Qt::CaseInsensitive) == 0);
        if (idMatch) {
            const QString candidatePath = QDir(m.path).absoluteFilePath(filename);
            if (m.files.contains(filename) && QFileInfo::exists(candidatePath)) {
                return true;
            }
        }
    }

    QDir root(m_modelsRoot);
    for (const QString &candidateDir : modelPathCandidates(modelId)) {
        if (candidateDir.isEmpty()) continue;
        if (QFileInfo::exists(root.absoluteFilePath(candidateDir + QStringLiteral("/") + filename))) {
            return true;
        }
    }

    return false;
}

QString ModelManager::filePath(const QString &modelId, const QString &filename) const
{
    const QString altId = QString(modelId).replace(QStringLiteral("/"), QStringLiteral("_"));
    const QString repoTail = modelId.contains(QStringLiteral("/")) ? modelId.section(QStringLiteral("/"), -1) : modelId;
    const QString repoTailNoGguf = repoTailWithoutGguf(modelId);

    for (const auto &m : m_models) {
        const QString dirName = QFileInfo(m.path).fileName();
        const bool idMatch =
            (m.id == modelId) ||
            (m.id == altId) ||
            (m.id.compare(repoTail, Qt::CaseInsensitive) == 0) ||
            (m.id.compare(repoTailNoGguf, Qt::CaseInsensitive) == 0) ||
            (dirName.compare(repoTail, Qt::CaseInsensitive) == 0) ||
            (dirName.compare(repoTailNoGguf, Qt::CaseInsensitive) == 0);
        if (idMatch) {
            const QString candidatePath = QDir(m.path).absoluteFilePath(filename);
            if (m.files.contains(filename) && QFileInfo::exists(candidatePath)) {
                return candidatePath;
            }
        }
    }

    QDir root(m_modelsRoot);
    for (const QString &candidateDir : modelPathCandidates(modelId)) {
        if (candidateDir.isEmpty()) continue;
        const QString candidatePath = root.absoluteFilePath(candidateDir + QStringLiteral("/") + filename);
        if (QFileInfo::exists(candidatePath)) {
            return candidatePath;
        }
    }

    return QString();
}

QVariantMap ModelManager::fileMetadata(const QString &modelId, const QString &filename) const
{
    const QString path = filePath(modelId, filename);
    if (path.isEmpty()) {
        return {};
    }

    QFileInfo fi(path);
    QFile infoFile(QDir(fi.absolutePath()).absoluteFilePath(QStringLiteral(".la-info.json")));
    if (!infoFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(infoFile.readAll());
    if (!doc.isObject()) {
        return {};
    }

    const QVariantMap info = doc.object().toVariantMap();
    const QVariantMap files = info.value(QStringLiteral("sourceFiles")).toMap();
    return files.value(filename).toMap();
}

bool ModelManager::setFileMetadata(const QString &modelId, const QString &filename, const QVariantMap &metadata)
{
    const QString path = filePath(modelId, filename);
    if (path.isEmpty()) {
        return false;
    }

    QFileInfo fi(path);
    QFile infoFile(QDir(fi.absolutePath()).absoluteFilePath(QStringLiteral(".la-info.json")));

    QJsonObject info;
    if (infoFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument existingDoc = QJsonDocument::fromJson(infoFile.readAll());
        if (existingDoc.isObject()) {
            info = existingDoc.object();
        }
        infoFile.close();
    }

    QJsonObject files = info.value(QStringLiteral("sourceFiles")).toObject();
    files.insert(filename, QJsonObject::fromVariantMap(metadata));
    info.insert(QStringLiteral("sourceFiles"), files);

    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Logger::error(QStringLiteral("ModelManager"),
                      QStringLiteral("Failed to write model metadata: %1").arg(infoFile.fileName()));
        return false;
    }

    infoFile.write(QJsonDocument(info).toJson(QJsonDocument::Indented));
    infoFile.close();
    return true;
}

QString ModelManager::concreteModelDir(const QString &modelId) const
{
    for (const auto &m : m_models) {
        if (m.id == modelId) {
            return m.path;
        }
    }
    return QDir(m_modelsRoot).absoluteFilePath(normalizedModelIdPath(modelId));
}

QString ModelManager::virtualModelDir(const QString &modelId) const
{
    return QDir(PathUtils::hubModelsDir()).absoluteFilePath(normalizedModelIdPath(modelId));
}

QVariantMap ModelManager::findModel(const QString &id) const
{
    for (const auto &m : m_models) {
        if (m.id == id) {
            QVariantMap map;
            map["id"] = m.id;
            map["task"] = m.task;
            map["format"] = m.format;
            map["path"] = m.path;
            map["files"] = m.files;
            map["size"] = m.size;
            map["arch"] = m.arch;
            map["params"] = m.params;
            map["publisher"] = m.publisher;
            map["quant"] = m.quant;
            map["modified"] = m.modified;
            return map;
        }
    }
    return QVariantMap();
}

QVariantList ModelManager::modelsForTask(const QString &task) const
{
    QVariantList list;
    for (const auto &m : m_models) {
        if (m.task == task) {
            QVariantMap map;
            map["id"] = m.id;
            map["task"] = m.task;
            map["format"] = m.format;
            map["path"] = m.path;
            map["files"] = m.files;
            map["size"] = m.size;
            map["arch"] = m.arch;
            map["params"] = m.params;
            map["publisher"] = m.publisher;
            map["quant"] = m.quant;
            map["modified"] = m.modified;
            list.append(map);
        }
    }
    return list;
}

QVariantList ModelManager::filteredSttModels(const QString &searchText) const
{
    QVariantList list;
    QString lowerSearch = searchText.toLower();

    for (const auto &m : m_models) {
        if (m.task != "stt") continue;

        if (m.files.isEmpty()) {
            QString displayName = m.id;
            if (!searchText.isEmpty() && !displayName.toLower().contains(lowerSearch)) {
                continue;
            }

            QVariantMap map;
            map["id"] = m.id;
            map["author"] = m.author.isEmpty() ? "local" : m.author;
            map["size"] = m.size;
            map["path"] = m.path;
            map["fileName"] = "";
            map["displayName"] = displayName;
            list.append(map);
        } else {
            for (const auto &f : m.files) {
                if (f.endsWith(".bin") || f.endsWith(".gguf")) {
                    QString displayName = m.id + " (" + f + ")";
                    if (!searchText.isEmpty() && !displayName.toLower().contains(lowerSearch)) {
                        continue;
                    }

                    QVariantMap map;
                    map["id"] = m.id;
                    map["author"] = m.author.isEmpty() ? "local" : m.author;
                    map["size"] = m.size;
                    map["path"] = m.path + "/" + f;
                    map["fileName"] = f;
                    map["displayName"] = displayName;
                    list.append(map);
                }
            }
        }
    }
    return list;
}

QVariantList ModelManager::filteredVoiceCloneModels(const QString &searchText, const QString &runtimeId)
{
    // Implementation placeholder
    Q_UNUSED(searchText)
    Q_UNUSED(runtimeId)
    return QVariantList();
}

void ModelManager::refreshVoiceCloneCache()
{
}

QString ModelManager::registryPath() const
{
    return m_modelsRoot + QStringLiteral("/registry.json");
}

void ModelManager::saveRegistry()
{
    QJsonArray arr;
    for (const auto &m : m_models) {
        QJsonObject obj;
        obj["id"] = m.id;
        obj["author"] = m.author;
        obj["task"] = m.task;
        obj["format"] = m.format;
        obj["path"] = m.path;
        obj["size"] = m.size;
        obj["arch"] = m.arch;
        obj["params"] = m.params;
        obj["publisher"] = m.publisher;
        obj["quant"] = m.quant;
        obj["modified"] = m.modified;
        
        QJsonArray files;
        for (const auto &f : m.files) files.append(f);
        obj["files"] = files;

        arr.append(obj);
    }

    QFile file(registryPath());
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(arr);
        file.write(doc.toJson());
    }
}

} // namespace LAStudio
