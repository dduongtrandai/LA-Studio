#include "StudioSessionViewModel.h"
#include "controllers/AppController.h"
#include "controllers/ModelSessionRegistry.h"
#include "controllers/IModelSession.h"
#include "core/StudioCapabilityRegistry.h"
#include "controllers/StudioConfigurationResolver.h"
#include "core/Logger.h"
#include "core/RegistryManager.h"
#include "core/HardwareManager.h"
#include <QFileInfo>
#include <QTimer>

namespace LAStudio {

StudioSessionViewModel::StudioSessionViewModel(QObject *parent)
    : QObject(parent)
{
    AppController *app = AppController::instance();
    if (app) {
        m_familiesModel = new CapabilityFamilyModel(app->models(), app->runtimes(), app->registry(), app->settings(), this);
        if (app->registry()) {
            m_repository = new StudioSelectionRepository(app->registry()->connectionName(), this);
            m_repository->migrateLegacySelectionsIfNeeded(app->settings());
        }
    } else {
        m_familiesModel = new CapabilityFamilyModel(nullptr, nullptr, nullptr, nullptr, this);
    }
    if (app && app->tts()) {
        connect(app->tts(), &TtsEngine::cpuUsageChanged, this, &StudioSessionViewModel::stateChanged);
        connect(app->tts(), &TtsEngine::memoryUsageChanged, this, &StudioSessionViewModel::stateChanged);
    }

    m_inferenceUiTimer = new QTimer(this);
    m_inferenceUiTimer->setInterval(1000);
    connect(m_inferenceUiTimer, &QTimer::timeout, this, &StudioSessionViewModel::stateChanged);
}

void StudioSessionViewModel::setCapabilityId(const QString &id)
{
    if (m_capabilityId == id) return;
    m_capabilityId = id;
    emit capabilityIdChanged();

    m_familiesModel->setCapability(m_capabilityId);
    initializeAction();
    syncSelectionFromSettings();
}

void StudioSessionViewModel::setAutoLoadOnSync(bool enabled)
{
    if (m_autoLoadOnSync == enabled) return;
    m_autoLoadOnSync = enabled;
    emit autoLoadOnSyncChanged();
}

void StudioSessionViewModel::initializeAction()
{
    if (m_action) {
        m_action->disconnect(this);
        m_action->deleteLater();
        m_action = nullptr;
    }

    m_action = StudioCapabilityRegistry::instance()->createAction(m_capabilityId, this);

    if (m_action) {
        connect(m_action, &IStudioAction::stateChanged, this, &StudioSessionViewModel::onActionStateChanged);
        connect(m_action, &IStudioAction::activeConfigurationChanged, this, &StudioSessionViewModel::onActionConfigurationChanged);
    }
}

QString StudioSessionViewModel::selectedFamilyId() const
{
    if (m_selectionCommitted && m_repository) {
        return m_repository->selectionFor(m_capabilityId).familyId;
    }
    return m_pendingFamilyId;
}

QString StudioSessionViewModel::runtimeId() const
{
    if (m_selectionCommitted && m_repository) {
        return m_repository->selectionFor(m_capabilityId).runtimeId;
    }
    return m_pendingRuntimeId;
}

QString StudioSessionViewModel::runtimeVersion() const
{
    if (m_selectionCommitted && m_repository) {
        return m_repository->selectionFor(m_capabilityId).runtimeVersion;
    }
    return m_pendingRuntimeVersion;
}

QVariantMap StudioSessionViewModel::selectedFiles() const
{
    if (m_selectionCommitted && m_repository) {
        const StudioConfiguration committed = m_repository->selectionFor(m_capabilityId);
        if (m_pendingFamilyId.isEmpty() || m_pendingFamilyId == committed.familyId) {
            return committed.selectedFiles;
        }
    }
    return m_pendingFiles;
}

bool StudioSessionViewModel::studioReady() const
{
    return modelActive();
}

bool StudioSessionViewModel::modelActive() const
{
    if (!m_action) return false;
    StudioState s = m_action->state();
    return s == StudioState::Ready || s == StudioState::Processing;
}

bool StudioSessionViewModel::canProcess() const
{
    return m_action && m_action->state() == StudioState::Ready;
}

QString StudioSessionViewModel::activeSignature() const
{
    return m_action ? m_action->activeConfigurationSignature() : QString();
}

QString StudioSessionViewModel::pendingSignature() const
{
    AppController *app = AppController::instance();
    if (app && app->sessionRegistry()) {
        IModelSession *session = app->sessionRegistry()->sessionForCapability(m_capabilityId);
        if (session && session->pendingConfiguration()) {
            return session->pendingConfiguration()->signature;
        }
    }
    return QString();
}

QString StudioSessionViewModel::lastError() const
{
    return m_action ? m_action->errorDetail() : QString();
}

StudioState StudioSessionViewModel::state() const
{
    return m_action ? m_action->state() : StudioState::Unloaded;
}

QString StudioSessionViewModel::statusText() const
{
    switch (state()) {
    case StudioState::Unloaded: return QStringLiteral("Unloaded");
    case StudioState::Loading: return QStringLiteral("Loading");
    case StudioState::Unloading: return QStringLiteral("Unloading");
    case StudioState::Ready: return QStringLiteral("Ready");
    case StudioState::Processing: return QStringLiteral("Processing");
    case StudioState::Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Unknown");
}

QString StudioSessionViewModel::statusDetail() const
{
    if (m_action && !m_action->errorDetail().isEmpty()) {
        return m_action->errorDetail();
    }
    if (m_selectionCommitted) {
        return studioReady() ? QStringLiteral("Loaded and ready") : QStringLiteral("Configuration saved (unloaded)");
    }
    return QStringLiteral("Configure a model family to start");
}

double StudioSessionViewModel::cpuUsage() const
{
    AppController *app = AppController::instance();
    const StudioCapabilityDescriptor desc = StudioCapabilityRegistry::instance()->getCapability(m_capabilityId);
    if (!app || desc.sharedEngineGroup != QStringLiteral("tts-shared")) return 0.0;
    return app->tts()->cpuUsage();
}

QString StudioSessionViewModel::estimatedRamUsage() const
{
    AppController *app = AppController::instance();
    const StudioCapabilityDescriptor desc = StudioCapabilityRegistry::instance()->getCapability(m_capabilityId);
    if (!app || desc.sharedEngineGroup != QStringLiteral("tts-shared")) return {};
    return app->tts()->estimatedRamUsage();
}

QString StudioSessionViewModel::estimatedVramUsage() const
{
    AppController *app = AppController::instance();
    const StudioCapabilityDescriptor desc = StudioCapabilityRegistry::instance()->getCapability(m_capabilityId);
    if (!app || desc.sharedEngineGroup != QStringLiteral("tts-shared")) return {};
    return app->tts()->estimatedVramUsage();
}

std::optional<SessionConfiguration> StudioSessionViewModel::activeSessionConfiguration() const
{
    AppController *app = AppController::instance();
    if (!app || !app->sessionRegistry()) {
        return std::nullopt;
    }

    IModelSession *session = app->sessionRegistry()->sessionForCapability(m_capabilityId);
    if (!session) {
        return std::nullopt;
    }

    auto config = session->activeConfiguration();
    if (!config || config->capabilityId != m_capabilityId) {
        return std::nullopt;
    }
    return config;
}

QString StudioSessionViewModel::loadedModelName() const
{
    auto config = activeSessionConfiguration();
    if (!config) {
        return QString();
    }

    const QVariantMap family = config->familyConfig.isEmpty()
        ? getFamilyConfig(config->selection.familyId)
        : config->familyConfig;
    return family.value(QStringLiteral("title"), config->selection.familyId).toString();
}

QVariantList StudioSessionViewModel::loadedModelFiles() const
{
    auto config = activeSessionConfiguration();
    if (!config) {
        return {};
    }

    return filesForConfiguration(*config);
}

QVariantList StudioSessionViewModel::filesForConfiguration(const SessionConfiguration &config) const
{
    QVariantList out;
    const QVariantMap family = config.familyConfig.isEmpty()
        ? getFamilyConfig(config.selection.familyId)
        : config.familyConfig;
    const QVariantList requiredFiles = family.value(QStringLiteral("requiredFiles")).toList();

    for (const QVariant &reqValue : requiredFiles) {
        const QVariantMap req = reqValue.toMap();
        const QString role = req.value(QStringLiteral("role")).toString();
        if (role.isEmpty() || !config.selection.selectedFiles.contains(role)) {
            continue;
        }

        const QString selectedFile = config.selection.selectedFiles.value(role).toString();
        const QString resolvedPath = config.resolvedPathsByRole.value(role).toString();
        QVariantMap item;
        item.insert(QStringLiteral("role"), role);
        item.insert(QStringLiteral("label"), req.value(QStringLiteral("name"), role).toString());
        item.insert(QStringLiteral("purpose"), req.value(QStringLiteral("purpose")).toString());
        item.insert(QStringLiteral("fileName"), selectedFile.isEmpty() ? QFileInfo(resolvedPath).fileName() : selectedFile);
        item.insert(QStringLiteral("path"), resolvedPath);
        out.append(item);
    }

    if (out.isEmpty()) {
        for (auto it = config.selection.selectedFiles.cbegin(); it != config.selection.selectedFiles.cend(); ++it) {
            const QString role = it.key();
            const QString resolvedPath = config.resolvedPathsByRole.value(role).toString();
            QVariantMap item;
            item.insert(QStringLiteral("role"), role);
            item.insert(QStringLiteral("label"), role);
            item.insert(QStringLiteral("purpose"), QString());
            item.insert(QStringLiteral("fileName"), it.value().toString());
            item.insert(QStringLiteral("path"), resolvedPath);
            out.append(item);
        }
    }

    return out;
}

QString StudioSessionViewModel::activeModelId() const
{
    auto config = activeSessionConfiguration();
    return config ? config->signature : QString();
}

QVariantList StudioSessionViewModel::loadedModels() const
{
    QVariantList out;
    AppController *app = AppController::instance();
    IModelSession *session = app && app->sessionRegistry()
        ? app->sessionRegistry()->sessionForCapability(m_capabilityId)
        : nullptr;
    if (!session) {
        return out;
    }

    const QString active = session->activeSignature();
    for (const SessionConfiguration &config : session->loadedConfigurations()) {
        if (config.capabilityId != m_capabilityId) {
            continue;
        }
        out.append(modelListItemForConfiguration(config, config.signature == active));
    }
    return out;
}

QVariantMap StudioSessionViewModel::modelListItemForConfiguration(const SessionConfiguration &config, bool active) const
{
    const QVariantMap family = config.familyConfig.isEmpty()
        ? getFamilyConfig(config.selection.familyId)
        : config.familyConfig;

    QString runtimeText = config.selection.runtimeId;
    const QVariantList runtimes = family.value(QStringLiteral("runtimes")).toList();
    for (const QVariant &rtVal : runtimes) {
        const QVariantMap rt = rtVal.toMap();
        if (rt.value(QStringLiteral("id")).toString() == config.selection.runtimeId) {
            runtimeText = rt.value(QStringLiteral("name"), config.selection.runtimeId).toString();
            if (!config.selection.runtimeVersion.isEmpty()) {
                runtimeText += QStringLiteral("  ") + config.selection.runtimeVersion;
            }
            break;
        }
    }

    QVariantMap item;
    item.insert(QStringLiteral("id"), config.signature);
    item.insert(QStringLiteral("familyId"), config.selection.familyId);
    item.insert(QStringLiteral("title"), family.value(QStringLiteral("title"), config.selection.familyId).toString());
    item.insert(QStringLiteral("modelId"), family.value(QStringLiteral("modelId"), config.selection.familyId).toString());
    item.insert(QStringLiteral("runtimeId"), config.selection.runtimeId);
    item.insert(QStringLiteral("runtimeVersion"), config.selection.runtimeVersion);
    item.insert(QStringLiteral("runtime"), runtimeText);
    item.insert(QStringLiteral("status"), active ? statusText() : QStringLiteral("Ready"));
    item.insert(QStringLiteral("active"), active);
    item.insert(QStringLiteral("fileCount"), filesForConfiguration(config).size());
    item.insert(QStringLiteral("estimatedRamUsage"), active ? estimatedRamUsage() : QString());
    item.insert(QStringLiteral("estimatedVramUsage"), active ? estimatedVramUsage() : QString());
    return item;
}

qint64 StudioSessionViewModel::inferenceElapsedMs() const
{
    if (state() == StudioState::Processing && m_inferenceTimer.isValid()) {
        return m_inferenceTimer.elapsed();
    }
    return m_lastInferenceElapsedMs;
}

QString StudioSessionViewModel::inferenceElapsedText() const
{
    const qint64 elapsed = inferenceElapsedMs();
    if (elapsed <= 0) {
        return QString();
    }
    return formatElapsed(elapsed);
}

QString StudioSessionViewModel::formatElapsed(qint64 elapsedMs)
{
    const qint64 totalSeconds = qMax<qint64>(0, elapsedMs / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QVariantMap StudioSessionViewModel::getFamilyConfig(const QString &familyId) const
{
    AppController* app = AppController::instance();
    if (!app) return {};

    const QString domain = StudioCapabilityRegistry::instance()->familyDomain(m_capabilityId);
    const QVariantList all = domain == QStringLiteral("stt")
        ? app->registry()->sttFamilies()
        : app->registry()->ttsFamilies();
    for (const QVariant &item : all) {
        QVariantMap fam = item.toMap();
        if (fam.value(QStringLiteral("id")).toString() == familyId) {
            return fam;
        }
    }
    return {};
}

QVariantList StudioSessionViewModel::families() const
{
    AppController* app = AppController::instance();
    if (!app) return {};

    const QString domain = StudioCapabilityRegistry::instance()->familyDomain(m_capabilityId);
    const QVariantList all = domain == QStringLiteral("stt")
        ? app->registry()->sttFamilies()
        : app->registry()->ttsFamilies();
    QVariantList out;
    for (const QVariant &item : all) {
        QVariantMap fam = item.toMap();
        if (StudioCapabilityRegistry::instance()->familySupportsCapability(fam, m_capabilityId)) {
            out.append(item);
        }
    }
    return out;
}

void StudioSessionViewModel::openConfiguration(const QString &familyId)
{
    selectFamily(familyId);
    emit configurationGalleryRequestReset();
}

void StudioSessionViewModel::selectFamily(const QString &familyId)
{
    m_pendingFamilyId = familyId;
    m_pendingFiles.clear();
    m_familiesModel->setSelectedFamilyId(m_pendingFamilyId);

    // Resolve compatible runtime default
    QVariantMap family = getFamilyConfig(familyId);
    QVariantList runtimes = family.value(QStringLiteral("runtimes")).toList();
    
    m_pendingRuntimeId.clear();
    m_pendingRuntimeVersion.clear();

    for (const QVariant &rtVal : runtimes) {
        QVariantMap rt = rtVal.toMap();
        QVariantMap comp = HardwareManager::instance()->runtimeCompatibility(rt);
        if (comp.value(QStringLiteral("compatible")).toBool()) {
            m_pendingRuntimeId = rt.value(QStringLiteral("id")).toString();
            m_pendingRuntimeVersion = rt.value(QStringLiteral("version")).toString();
            break;
        }
    }

    emit selectionChanged();
}

void StudioSessionViewModel::selectRuntime(const QString &runtimeId, const QString &version)
{
    m_pendingRuntimeId = runtimeId;
    m_pendingRuntimeVersion = version;
    emit selectionChanged();
}

void StudioSessionViewModel::commitSelection()
{
    commitSelectionInternal(true);
}

void StudioSessionViewModel::commitSelectionInternal(bool loadAfterCommit)
{
    if (m_pendingFamilyId.isEmpty()) return;

    StudioConfiguration config;
    config.capabilityId = m_capabilityId;
    config.familyId = m_pendingFamilyId;
    config.runtimeId = m_pendingRuntimeId;
    config.runtimeVersion = m_pendingRuntimeVersion;

    // Build requirement selections
    QVariantMap family = getFamilyConfig(m_pendingFamilyId);
    QVariantList reqFiles = family.value(QStringLiteral("requiredFiles")).toList();
    AppController* app = AppController::instance();
    
    for (const QVariant &reqVal : reqFiles) {
        QVariantMap req = reqVal.toMap();
        QString role = req.value(QStringLiteral("role")).toString();
        
        QString selected = m_pendingFiles.value(role).toString();
        if (selected.isEmpty()) {
            // Find the best installed candidate when the user did not choose one.
            selected = req.value(QStringLiteral("file")).toString();
            QVariantList candidates = req.value(QStringLiteral("candidates")).toList();
            for (const QVariant &cand : candidates) {
                QString modelId = req.value(QStringLiteral("modelId")).toString();
                if (modelId.isEmpty()) modelId = family.value(QStringLiteral("modelId")).toString();
                if (app->models()->hasFile(modelId, cand.toString())) {
                    selected = cand.toString();
                    break;
                }
            }
        }
        config.selectedFiles.insert(role, selected);
    }

    if (m_repository) {
        m_repository->saveActiveSelection(config);
    }

    m_selectionCommitted = true;
    emit selectionChanged();
    emit studioContextChanged(config.familyId, config.runtimeId, config.runtimeVersion);
    emit configurationDialogClosed();

    if (loadAfterCommit) {
        loadSelectedConfiguration();
    }
}

void StudioSessionViewModel::loadSelectedConfiguration()
{
    if (!m_action || !m_repository) return;
    StudioConfiguration config = m_repository->selectionFor(m_capabilityId);
    if (config.isValid()) {
        m_action->load(config);
    }
}

void StudioSessionViewModel::unload()
{
    if (m_action) {
        m_action->unload();
    }
}

void StudioSessionViewModel::reload()
{
    if (m_action) {
        if (state() == StudioState::Unloaded || state() == StudioState::Error) {
            loadSelectedConfiguration();
        } else {
            m_action->reload();
        }
    }
}

void StudioSessionViewModel::activateLoadedModel(const QString &modelId)
{
    if (modelId.isEmpty() || modelId == activeModelId()) {
        return;
    }

    AppController *app = AppController::instance();
    IModelSession *session = app && app->sessionRegistry()
        ? app->sessionRegistry()->sessionForCapability(m_capabilityId)
        : nullptr;
    if (!session) {
        return;
    }

    session->activateConfiguration(modelId);
    auto active = session->activeConfiguration();
    if (active) {
        m_pendingFamilyId = active->selection.familyId;
        m_pendingRuntimeId = active->selection.runtimeId;
        m_pendingRuntimeVersion = active->selection.runtimeVersion;
        m_pendingFiles = active->selection.selectedFiles;
        m_selectionCommitted = true;
        if (m_familiesModel) {
            m_familiesModel->setSelectedFamilyId(m_pendingFamilyId);
        }
        if (m_repository) {
            m_repository->saveActiveSelection(active->selection);
        }
        emit selectionChanged();
        emit studioContextChanged(active->selection.familyId, active->selection.runtimeId, active->selection.runtimeVersion);
    }
    emit stateChanged();
}

void StudioSessionViewModel::unloadLoadedModel(const QString &modelId)
{
    if (modelId.isEmpty()) {
        return;
    }

    AppController *app = AppController::instance();
    IModelSession *session = app && app->sessionRegistry()
        ? app->sessionRegistry()->sessionForCapability(m_capabilityId)
        : nullptr;
    if (!session) {
        return;
    }
    session->requestUnloadConfiguration(modelId);
    emit stateChanged();
}

void StudioSessionViewModel::syncSelectionFromSettings()
{
    if (!m_repository) return;
    StudioConfiguration config = m_repository->selectionFor(m_capabilityId);
    if (config.isValid()) {
        m_pendingFamilyId = config.familyId;
        m_pendingRuntimeId = config.runtimeId;
        m_pendingRuntimeVersion = config.runtimeVersion;
        m_pendingFiles = config.selectedFiles;
        m_familiesModel->setSelectedFamilyId(m_pendingFamilyId);
        m_selectionCommitted = true;
        
        emit selectionChanged();
        emit studioContextChanged(m_pendingFamilyId, m_pendingRuntimeId, m_pendingRuntimeVersion);

    } else {
        m_selectionCommitted = false;
        m_pendingFamilyId.clear();
        m_pendingRuntimeId.clear();
        m_pendingRuntimeVersion.clear();
        m_pendingFiles.clear();
        m_familiesModel->setSelectedFamilyId(QString());
        emit selectionChanged();
    }
}

void StudioSessionViewModel::commitConfigurationSelection(const QString &familyId, const QString &runtimeId, const QString &runtimeVersion, const QVariantMap &selectedFiles)
{
    m_pendingFamilyId = familyId;
    m_pendingRuntimeId = runtimeId;
    m_pendingRuntimeVersion = runtimeVersion;
    m_pendingFiles = selectedFiles;
    commitSelection();
}

void StudioSessionViewModel::saveConfigurationSelection(const QString &familyId, const QString &runtimeId, const QString &runtimeVersion, const QVariantMap &selectedFiles)
{
    m_pendingFamilyId = familyId;
    m_pendingRuntimeId = runtimeId;
    m_pendingRuntimeVersion = runtimeVersion;
    m_pendingFiles = selectedFiles;
    commitSelectionInternal(false);
}

void StudioSessionViewModel::autoLoadIfReady()
{
    // Kept for older QML bindings. Opening a studio must not load a model;
    // loading should only happen from an explicit user action.
}

void StudioSessionViewModel::onActionStateChanged()
{
    syncInferenceTimer();
    emit stateChanged();
}

void StudioSessionViewModel::onActionConfigurationChanged()
{
    emit stateChanged();
}

void StudioSessionViewModel::syncInferenceTimer()
{
    const StudioState currentState = state();
    if (currentState == StudioState::Processing) {
        if (!m_inferenceTimer.isValid()) {
            m_lastInferenceElapsedMs = 0;
            m_inferenceTimer.start();
        }
        if (m_inferenceUiTimer && !m_inferenceUiTimer->isActive()) {
            m_inferenceUiTimer->start();
        }
        return;
    }

    if (m_inferenceTimer.isValid()) {
        m_lastInferenceElapsedMs = m_inferenceTimer.elapsed();
        m_inferenceTimer.invalidate();
    }
    if (m_inferenceUiTimer) {
        m_inferenceUiTimer->stop();
    }

    if (currentState == StudioState::Loading ||
        currentState == StudioState::Unloading ||
        currentState == StudioState::Unloaded ||
        currentState == StudioState::Error)
    {
        m_lastInferenceElapsedMs = 0;
    }
}

// Presentational helper implementations
QString StudioSessionViewModel::studioHeaderTitle() const
{
    StudioCapabilityDescriptor desc = StudioCapabilityRegistry::instance()->getCapability(m_capabilityId);
    return desc.pageTitle;
}

QString StudioSessionViewModel::modalSelectionTitle() const
{
    QVariantMap family = getFamilyConfig(selectedFamilyId());
    return family.value(QStringLiteral("title"), QStringLiteral("Model + Runtime")).toString();
}

QString StudioSessionViewModel::modalSelectionValue() const
{
    return runtimeDisplayText();
}

QString StudioSessionViewModel::modalSelectionDetail() const
{
    return statusDetail();
}

QString StudioSessionViewModel::runtimeDisplayText() const
{
    QVariantMap family = getFamilyConfig(selectedFamilyId());
    QString rtId = runtimeId();
    QString rtVer = runtimeVersion();
    if (rtId.isEmpty()) return QStringLiteral("Select model and runtime");

    QVariantList runtimes = family.value(QStringLiteral("runtimes")).toList();
    for (const QVariant &rtVal : runtimes) {
        QVariantMap rt = rtVal.toMap();
        if (rt.value(QStringLiteral("id")).toString() == rtId) {
            QString name = rt.value(QStringLiteral("name")).toString();
            return rtVer.isEmpty() ? name : name + QStringLiteral("  ") + rtVer;
        }
    }
    return rtId;
}

} // namespace LAStudio
