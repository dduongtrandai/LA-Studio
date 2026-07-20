#include "controllers/app/AppController.h"

#include "core/Settings.h"
#include "controllers/models/ModelSessionRegistry.h"
#include "core/StudioSelectionRepository.h"
#include "controllers/app/WorkflowActivityManager.h"
#include "controllers/translation/TranslationModelSession.h"
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
#include "controllers/shared/AppUpdateService.h"
#include "api/ApiServerService.h"
#include <QGuiApplication>
#include <QClipboard>
#include "core/Logger.h"
#include <QTimer>

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
    m_translationEngine = new TranslationEngine({}, this);
    m_runtimes  = new RuntimeManager(m_catalog, m_settings, this);
    m_alignment = new AlignmentExecutionService(m_runtimes, m_models, this);
    m_voiceIsolator = new VoiceIsolatorController(this);
    m_sessionRegistry = new ModelSessionRegistry(m_stt, m_tts, m_translationEngine, m_alignment, m_voiceIsolator, this);
    m_translation = new TranslationController(m_translationEngine,
        qobject_cast<TranslationModelSession*>(m_sessionRegistry->sessionForCapability(QStringLiteral("translation"))), this);
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
    m_subtitleVoice = new SubtitleVoiceController(m_tts, m_player, m_history, this);
    m_dubbing = new DubbingController(m_sttSession, m_tts, m_translationEngine, m_models, m_runtimes, this);
    m_updates = new AppUpdateService(m_downloads, this);
    m_examples = new ExampleManager(this);
    m_workflows = new WorkflowActivityManager(m_sessionRegistry, m_tts, m_sttSession, m_alignment, m_dubbing, this);
    m_apiServer = new ApiServerService(m_settings, m_tts, m_stt, this);

    connect(m_preview, &AudioPreviewService::errorOccurred, this, &AppController::onError);
    connect(m_player, &AudioPlayer::errorOccurred, this, &AppController::onError);
    connect(m_history, &HistoryService::errorOccurred, this, &AppController::onError);
    connect(m_downloadInstall, &DownloadInstallService::errorOccurred, this, &AppController::onError);
    connect(m_alignment, &AlignmentExecutionService::failed, this,
            [this](const QString &, const QString &message) { onError(message); });
    connect(m_translation, &TranslationController::errorTextChanged, this, [this]() {
        if (m_translation && !m_translation->errorText().isEmpty()) onError(m_translation->errorText());
    });
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

    QTimer::singleShot(2000, this, [this]() {
        if (m_updates) {
            m_updates->checkForUpdates(QStringLiteral("stable"));
        }
    });
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
