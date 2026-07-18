#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

#include "core/Settings.h"
#include "core/LocalizationManager.h"
#include "core/HFHubClient.h"
#include "core/DownloadManager.h"
#include "core/ModelManager.h"
#include "core/CatalogManager.h"
#include "core/RegistryManager.h"
#include "core/RuntimeManager.h"
#include "core/LogViewService.h"
#include "stt/SttEngine.h"
#include "tts/TtsEngine.h"
#include "translation/TranslationEngine.h"
#include "audio/AudioRecorder.h"
#include "audio/AudioPlayer.h"
#include "audio/WaveformProvider.h"
#include "controllers/dubbing/DubbingController.h"
#include "AudioPreviewService.h"
#include "HistoryService.h"
#include "ModelsPathMigrationService.h"
#include "FileAccessService.h"
#include "DownloadInstallService.h"
#include "controllers/alignment/AlignmentExecutionService.h"
#include "TranslationController.h"
#include "VoiceClonePresetService.h"
#include "VoiceDesignPresetService.h"
#include "SttSessionController.h"
#include "SubtitleVoiceController.h"
#include "VoiceIsolatorController.h"
#include "AppUpdateService.h"
#include "ExampleManager.h"
#include "controllers/app/WorkflowActivityManager.h"
#include "api/ApiServerService.h"

#include "ModelSessionRegistry.h"

namespace LAStudio {
 
class AppController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Settings*        settings  READ settings  CONSTANT)
    Q_PROPERTY(LocalizationManager* localization READ localization CONSTANT)
    Q_PROPERTY(HFHubClient*     hub       READ hub       CONSTANT)
    Q_PROPERTY(DownloadManager* downloads READ downloads CONSTANT)
    Q_PROPERTY(ModelManager*    models    READ models    CONSTANT)
    Q_PROPERTY(CatalogManager*  catalog   READ catalog   CONSTANT)
    Q_PROPERTY(RegistryManager* registry  READ registry  CONSTANT)
    Q_PROPERTY(RuntimeManager*  runtimes  READ runtimes  CONSTANT)
    Q_PROPERTY(LogViewService*  logs      READ logs      CONSTANT)
    Q_PROPERTY(SttEngine*       stt       READ stt       CONSTANT)
    Q_PROPERTY(TtsEngine*       tts       READ tts       CONSTANT)
    Q_PROPERTY(TranslationEngine* translationEngine READ translationEngine CONSTANT)
    Q_PROPERTY(AudioRecorder*   recorder  READ recorder  CONSTANT)
    Q_PROPERTY(AudioPlayer*     player    READ player    CONSTANT)
    Q_PROPERTY(AudioPreviewService* preview READ preview CONSTANT)
    Q_PROPERTY(HistoryService*  history   READ history   CONSTANT)
    Q_PROPERTY(ModelsPathMigrationService* modelsMigration READ modelsMigration CONSTANT)
    Q_PROPERTY(FileAccessService* files READ files CONSTANT)
    Q_PROPERTY(DownloadInstallService* downloadInstall READ downloadInstall CONSTANT)
    Q_PROPERTY(AlignmentExecutionService* alignment READ alignment CONSTANT)
    Q_PROPERTY(TranslationController* translation READ translation CONSTANT)
    Q_PROPERTY(VoiceClonePresetService* voiceClonePresets READ voiceClonePresets CONSTANT)
    Q_PROPERTY(VoiceDesignPresetService* voiceDesignPresets READ voiceDesignPresets CONSTANT)
    Q_PROPERTY(SttSessionController* sttSession READ sttSession CONSTANT)
    Q_PROPERTY(SubtitleVoiceController* subtitleVoice READ subtitleVoice CONSTANT)
    Q_PROPERTY(DubbingController* dubbing READ dubbing CONSTANT)
    Q_PROPERTY(VoiceIsolatorController* voiceIsolator READ voiceIsolator CONSTANT)
    Q_PROPERTY(AppUpdateService* updates READ updates CONSTANT)
    Q_PROPERTY(ExampleManager* examples READ examples CONSTANT)
    Q_PROPERTY(ModelSessionRegistry* sessionRegistry READ sessionRegistry CONSTANT)
    Q_PROPERTY(WorkflowActivityManager* workflows READ workflows CONSTANT)
    Q_PROPERTY(ApiServerService* apiServer READ apiServer CONSTANT)

    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString logsDir READ logsDir CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    static AppController *instance();
    static AppController *create(QQmlEngine *, QJSEngine *);

    Settings*        settings()  const { return m_settings; }
    LocalizationManager* localization() const { return m_localization; }
    HFHubClient*     hub()       const { return m_hub; }
    DownloadManager* downloads() const { return m_downloads; }
    ModelManager*    models()    const { return m_models; }
    CatalogManager*  catalog()   const { return m_catalog; }
    RegistryManager* registry()  const { return m_registry; }
    RuntimeManager*  runtimes()  const { return m_runtimes; }
    LogViewService*  logs()      const { return m_logs; }
    SttEngine*       stt()       const { return m_stt; }
    TtsEngine*       tts()       const { return m_tts; }
    TranslationEngine* translationEngine() const { return m_translationEngine; }
    AudioRecorder*   recorder()  const { return m_recorder; }
    AudioPlayer*     player()    const { return m_player; }
    AudioPreviewService* preview() const { return m_preview; }
    HistoryService*  history()   const { return m_history; }
    ModelsPathMigrationService* modelsMigration() const { return m_modelsMigration; }
    FileAccessService* files() const { return m_files; }
    DownloadInstallService* downloadInstall() const { return m_downloadInstall; }
    AlignmentExecutionService* alignment() const { return m_alignment; }
    TranslationController* translation() const { return m_translation; }
    VoiceClonePresetService* voiceClonePresets() const { return m_voiceClonePresets; }
    VoiceDesignPresetService* voiceDesignPresets() const { return m_voiceDesignPresets; }
    SttSessionController* sttSession() const { return m_sttSession; }
    SubtitleVoiceController* subtitleVoice() const { return m_subtitleVoice; }
    DubbingController* dubbing() const { return m_dubbing; }
    VoiceIsolatorController* voiceIsolator() const { return m_voiceIsolator; }
    AppUpdateService* updates() const { return m_updates; }
    ExampleManager* examples() const { return m_examples; }
    ModelSessionRegistry* sessionRegistry() const { return m_sessionRegistry; }
    WorkflowActivityManager* workflows() const { return m_workflows; }
    ApiServerService* apiServer() const { return m_apiServer; }

    WaveformProvider* waveformProvider() const { return m_waveformProvider; }

    QString errorMessage() const { return m_errorMessage; }
    QString logsDir() const;
    QString dataDir() const;

    Q_INVOKABLE void clearError();
    Q_INVOKABLE void copyToClipboard(const QString &text);

signals:
    void errorMessageChanged();

private slots:
    void onError(const QString &msg);

private:
    static AppController *s_instance;

    Settings*        m_settings = nullptr;
    LocalizationManager* m_localization = nullptr;
    HFHubClient*     m_hub = nullptr;
    DownloadManager* m_downloads = nullptr;
    ModelManager*    m_models = nullptr;
    CatalogManager*  m_catalog = nullptr;
    RegistryManager* m_registry = nullptr;
    RuntimeManager*  m_runtimes = nullptr;
    LogViewService*  m_logs = nullptr;
    SttEngine*       m_stt = nullptr;
    TtsEngine*       m_tts = nullptr;
    TranslationEngine* m_translationEngine = nullptr;
    AudioRecorder*   m_recorder = nullptr;
    AudioPlayer*     m_player = nullptr;
    WaveformProvider* m_waveformProvider = nullptr;
    AudioPreviewService* m_preview = nullptr;
    HistoryService*  m_history = nullptr;
    ModelsPathMigrationService* m_modelsMigration = nullptr;
    FileAccessService* m_files = nullptr;
    DownloadInstallService* m_downloadInstall = nullptr;
    AlignmentExecutionService* m_alignment = nullptr;
    TranslationController* m_translation = nullptr;
    VoiceClonePresetService* m_voiceClonePresets = nullptr;
    VoiceDesignPresetService* m_voiceDesignPresets = nullptr;
    SttSessionController* m_sttSession = nullptr;
    SubtitleVoiceController* m_subtitleVoice = nullptr;
    DubbingController* m_dubbing = nullptr;
    VoiceIsolatorController* m_voiceIsolator = nullptr;
    AppUpdateService* m_updates = nullptr;
    ExampleManager* m_examples = nullptr;
    ModelSessionRegistry* m_sessionRegistry = nullptr;
    WorkflowActivityManager* m_workflows = nullptr;
    ApiServerService* m_apiServer = nullptr;

    QString m_errorMessage;
};

} // namespace LAStudio
