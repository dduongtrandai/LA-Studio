#include "AppController.h"

#include "core/Settings.h"
#include "controllers/ModelSessionRegistry.h"
#include "core/StudioSelectionRepository.h"
#include "controllers/WorkflowActivityManager.h"
#include "core/HFHubClient.h"
#include "core/DownloadManager.h"
#include "core/ModelManager.h"
#include "core/RegistryManager.h"
#include "core/PathUtils.h"
#include "stt/SttEngine.h"
#include "tts/TtsEngine.h"
#include "audio/AudioRecorder.h"
#include "audio/AudioPlayer.h"
#include "audio/WaveformProvider.h"
#include "controllers/AppUpdateService.h"
#include "api/ApiServerService.h"
#include <QGuiApplication>
#include <QClipboard>
#include "core/Logger.h"
#include <QTimer>
#include <QFileInfo>

namespace LAStudio {

AppController *AppController::s_instance = nullptr;

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    s_instance = this;

    PathUtils::ensureDirsExist();

    m_settings  = new Settings(this);
    m_localization = new LocalizationManager(m_settings, this);
    m_hub       = new HFHubClient(this);
    m_downloads = new DownloadManager(m_hub, this);
    m_models    = new ModelManager(this);
    m_models->setModelsRoot(m_settings->modelsPath());
    m_models->scanLocalModels();
    m_catalog   = new CatalogManager(this);
    m_registry  = new RegistryManager(this);
    m_registry->initializeFromCatalog(m_catalog);
    m_logs      = new LogViewService(this);
    m_stt       = new SttEngine(this);
    m_tts       = new TtsEngine(this);
    m_runtimes  = new RuntimeManager(m_catalog, m_settings, this);
    m_alignment = new AlignmentExecutionService(m_runtimes, m_models, this);
    m_voiceIsolator = new VoiceIsolatorController(this);
    m_sessionRegistry = new ModelSessionRegistry(m_stt, m_tts, m_alignment, m_voiceIsolator, this);
    m_recorder  = new AudioRecorder(this);
    m_player    = new AudioPlayer(this);
    m_waveformProvider = new WaveformProvider();

    m_preview   = new AudioPreviewService(m_tts, m_player, m_waveformProvider, this);
    m_history   = new HistoryService(m_tts, m_recorder, this);
    m_modelsMigration = new ModelsPathMigrationService(m_settings, m_models, m_downloads, m_stt, m_tts, this);
    m_files     = new FileAccessService(this);
    m_downloadInstall = new DownloadInstallService(m_downloads, m_models, m_runtimes, this);
    m_voiceClonePresets = new VoiceClonePresetService(this);
    m_voiceDesignPresets = new VoiceDesignPresetService(this);
    m_sttSession = new SttSessionController(this);
    m_dubbing = new DubbingController(m_sttSession, m_tts, m_models, m_runtimes, this);
    m_updates = new AppUpdateService(m_downloads, this);
    m_examples = new ExampleManager(this);
    m_workflows = new WorkflowActivityManager(m_sessionRegistry, m_tts, m_sttSession, m_alignment, m_dubbing, this);
    m_apiServer = new ApiServerService(m_settings, m_tts, m_stt, this);

    connect(m_preview, &AudioPreviewService::errorOccurred, this, &AppController::onError);
    connect(m_history, &HistoryService::errorOccurred, this, &AppController::onError);
    connect(m_downloadInstall, &DownloadInstallService::errorOccurred, this, &AppController::onError);
    connect(m_alignment, &AlignmentExecutionService::failed, this,
            [this](const QString &, const QString &message) { onError(message); });
    connect(m_voiceClonePresets, &VoiceClonePresetService::errorOccurred, this, &AppController::onError);
    connect(m_voiceDesignPresets, &VoiceDesignPresetService::errorOccurred, this, &AppController::onError);
    connect(m_updates, &AppUpdateService::errorOccurred, this, &AppController::onError);

    connect(m_stt, &SttEngine::errorOccurred, this, &AppController::onError);
    connect(m_tts, &TtsEngine::errorOccurred, this, &AppController::onError);
    connect(m_hub, &HFHubClient::searchError, this, &AppController::onError);
    connect(m_downloads, &DownloadManager::error, this,
            [this](const QString &, const QString &, const QString &err) { onError(err); });

    connect(m_settings, &Settings::modelsPathChanged, this, [this]() {
        m_models->setModelsRoot(m_settings->modelsPath());
        m_models->scanLocalModels();
    });

    // Dubbing and the standalone transcript page share the same STT session.
    // Load the selected/default model once at startup so a workflow cannot reach
    // inference with an empty active instance.
    QTimer::singleShot(0, this, &AppController::loadDefaultSttModel);
    connect(m_runtimes, &RuntimeManager::registryUpdated,
            this, &AppController::loadDefaultSttModel);

    QTimer::singleShot(2000, this, [this]() {
        if (m_updates) {
            m_updates->checkForUpdates(QStringLiteral("stable"));
        }
    });
}

void AppController::loadDefaultSttModel()
{
    if (!m_sessionRegistry || !m_registry || !m_settings) return;
    IModelSession *session = m_sessionRegistry->sessionForCapability(QStringLiteral("stt"));
    if (!session || session->modelActive()
        || session->state() == ModelSessionState::Loading
        || session->state() == ModelSessionState::Processing) return;

    QVariantMap family;
    const QString configuredFamily = m_settings->selectedSttFamily();
    // Migrate the previous bundled STT default. Any other non-empty value is
    // treated as an explicit user choice and remains untouched.
    const QString preferredFamily = configuredFamily.isEmpty()
        || configuredFamily == QStringLiteral("qwen3-asr-0.6b")
        ? QStringLiteral("nemotron-3.5-asr-streaming-0.6b")
        : configuredFamily;
    for (const QVariant &entry : m_registry->sttFamilies()) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == preferredFamily) {
            family = candidate;
            break;
        }
    }
    if (family.isEmpty()) {
        Logger::warning(QStringLiteral("AppController"),
                        QStringLiteral("Default STT family is unavailable: %1").arg(preferredFamily));
        return;
    }

    QVariantMap runtime;
    const QVariantList runtimes = family.value(QStringLiteral("runtimes")).toList();
    for (const QVariant &entry : runtimes) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == m_settings->selectedSttRuntime()) {
            runtime = candidate;
            break;
        }
    }
    if (runtime.isEmpty() && !runtimes.isEmpty()) runtime = runtimes.first().toMap();
    if (runtime.isEmpty()) return;

    StudioConfiguration configuration;
    configuration.capabilityId = QStringLiteral("stt");
    configuration.familyId = family.value(QStringLiteral("id")).toString();
    configuration.runtimeId = runtime.value(QStringLiteral("id")).toString();
    configuration.runtimeVersion = runtime.value(QStringLiteral("version")).toString();
    for (const QVariant &entry : family.value(QStringLiteral("requiredFiles")).toList()) {
        const QVariantMap file = entry.toMap();
        configuration.selectedFiles.insert(file.value(QStringLiteral("role")).toString(),
                                           file.value(QStringLiteral("file")).toString());
    }

    const QString runtimePath = m_runtimes->getRuntimePathForVersion(
        configuration.runtimeId, configuration.runtimeVersion);
    if (runtimePath.isEmpty() || !QFileInfo::exists(runtimePath)) {
        Logger::info(QStringLiteral("AppController"),
                     QStringLiteral("Deferring default STT load until runtime scan completes: %1 %2")
                         .arg(configuration.runtimeId, configuration.runtimeVersion));
        return;
    }

    m_settings->setSelectedSttFamily(configuration.familyId);
    m_settings->setSelectedSttRuntime(configuration.runtimeId);
    m_settings->setSelectedSttRuntimeVersion(configuration.runtimeVersion);
    Logger::info(QStringLiteral("AppController"),
                 QStringLiteral("Loading default STT model family=%1 runtime=%2 version=%3")
                     .arg(configuration.familyId, configuration.runtimeId, configuration.runtimeVersion));
    session->requestLoad(QStringLiteral("stt"), configuration);
}

AppController::~AppController()
{
    s_instance = nullptr;
}

AppController *AppController::instance()
{
    return s_instance;
}

AppController *AppController::create(QQmlEngine *, QJSEngine *)
{
    if (!s_instance) {
        s_instance = new AppController;
    }
    return s_instance;
}

void AppController::onError(const QString &msg)
{
    m_errorMessage = msg;
    emit errorMessageChanged();
}

void AppController::clearError()
{
    m_errorMessage.clear();
    emit errorMessageChanged();
}

void AppController::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

QString AppController::logsDir() const
{
    return PathUtils::logsDir();
}

QString AppController::dataDir() const
{
    return PathUtils::dataDir();
}

} // namespace LAStudio
