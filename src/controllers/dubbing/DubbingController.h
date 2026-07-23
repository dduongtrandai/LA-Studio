#pragma once

#include <QObject>
#include <QFileInfo>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>
#include <QtQml/qqml.h>
#include <memory>

#include "dubbing/DubbingProject.h"
#include "workflows/NodeRegistry.h"
#include "workflows/WorkflowGraphRunner.h"

namespace LAStudio {

class SttSessionController;
class TtsEngine;
class ModelManager;
class RuntimeManager;
class TranslationEngine;
class DubbingJobRunner;
class DubbingTranslationFixService;
class CapabilityFamilyModel;

class DubbingController : public QObject
{
    Q_OBJECT
    QML_UNCREATABLE("DubbingController is managed by AppController")

    Q_PROPERTY(bool hasProject READ hasProject NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(QString sourceMediaPath READ sourceMediaPath NOTIFY projectChanged)
    Q_PROPERTY(QUrl sourceMediaUrl READ sourceMediaUrl NOTIFY projectChanged)
    Q_PROPERTY(QUrl playbackMediaUrl READ playbackMediaUrl NOTIFY previewChanged)
    Q_PROPERTY(QString normalizedAudioPath READ normalizedAudioPath NOTIFY projectChanged)
    Q_PROPERTY(QString vocalsPath READ vocalsPath NOTIFY projectChanged)
    Q_PROPERTY(QString backgroundPath READ backgroundPath NOTIFY projectChanged)
    Q_PROPERTY(QString sourceLanguage READ sourceLanguage WRITE setSourceLanguage NOTIFY projectChanged)
    Q_PROPERTY(QString targetLanguage READ targetLanguage WRITE setTargetLanguage NOTIFY projectChanged)
    Q_PROPERTY(QVariantMap durationControl READ durationControl WRITE setDurationControl NOTIFY projectChanged)
    Q_PROPERTY(QVariantList speakers READ speakers NOTIFY projectChanged)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY segmentsChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY processingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
    Q_PROPERTY(QString previewPath READ previewPath NOTIFY previewChanged)
    Q_PROPERTY(QString dubbedVocalPath READ dubbedVocalPath NOTIFY previewChanged)
    Q_PROPERTY(QString exportPath READ exportPath NOTIFY exportChanged)
    Q_PROPERTY(QVariantList workflowNodes READ workflowNodes NOTIFY workflowChanged)
    Q_PROPERTY(QVariantMap workflowNodeConfigurations READ workflowNodeConfigurations NOTIFY workflowChanged)
    Q_PROPERTY(bool workflowReady READ workflowReady NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowStatusText READ workflowStatusText NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowId READ workflowId CONSTANT)
    Q_PROPERTY(int workflowVersion READ workflowVersion CONSTANT)
    Q_PROPERTY(bool workflowGraphValid READ workflowGraphValid NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowRunId READ workflowRunId NOTIFY processingChanged)
    Q_PROPERTY(QString workflowNodeRunId READ workflowNodeRunId NOTIFY processingChanged)
    Q_PROPERTY(bool workflowWaitingForInput READ workflowWaitingForInput NOTIFY workflowChanged)
    Q_PROPERTY(QVariantMap workflowReviewRequest READ workflowReviewRequest NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowMode READ workflowMode NOTIFY workflowChanged)
    Q_PROPERTY(QString currentStepId READ currentStepId NOTIFY workflowChanged)
    Q_PROPERTY(QVariantMap currentStepOutput READ currentStepOutput NOTIFY workflowChanged)
    Q_PROPERTY(QString lastCompletedStepId READ lastCompletedStepId NOTIFY workflowChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool translationFixing READ translationFixing NOTIFY translationFixChanged)
    Q_PROPERTY(int translationFixProgress READ translationFixProgress NOTIFY translationFixChanged)
    Q_PROPERTY(QString translationFixStatus READ translationFixStatus NOTIFY translationFixChanged)
    Q_PROPERTY(QVariantMap translationFixConfiguration READ translationFixConfiguration NOTIFY translationFixChanged)
    Q_PROPERTY(int translationFixCandidateCount READ translationFixCandidateCount NOTIFY segmentsChanged)
    Q_PROPERTY(QString dubbingQuality READ dubbingQuality WRITE setDubbingQuality NOTIFY projectChanged)
    Q_PROPERTY(QString adaptiveProvider READ adaptiveProvider NOTIFY translationFixChanged)
    Q_PROPERTY(bool adaptiveReady READ adaptiveReady NOTIFY workflowChanged)
    Q_PROPERTY(QString adaptiveStatusText READ adaptiveStatusText NOTIFY workflowChanged)
    Q_PROPERTY(bool customReady READ customReady NOTIFY workflowChanged)
    Q_PROPERTY(QString customStatusText READ customStatusText NOTIFY workflowChanged)
    Q_PROPERTY(bool settingsLocked READ settingsLocked NOTIFY workflowChanged)
    Q_PROPERTY(bool automaticSetupActive READ automaticSetupActive NOTIFY workflowChanged)
    Q_PROPERTY(QString automaticStatusText READ automaticStatusText NOTIFY workflowChanged)
    Q_PROPERTY(QVariantList automaticEvents READ automaticEvents NOTIFY workflowChanged)

public:
    explicit DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                               TranslationEngine *translation,
                               ModelManager *models = nullptr, RuntimeManager *runtimes = nullptr,
                               QObject *parent = nullptr);
    DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                      ModelManager *models = nullptr, RuntimeManager *runtimes = nullptr,
                      QObject *parent = nullptr);
    ~DubbingController() override;

    bool hasProject() const { return !m_project.projectPath.isEmpty(); }
    QString projectPath() const { return m_project.projectPath; }
    QString sourceMediaPath() const { return m_project.sourceMediaPath; }
    QString normalizedAudioPath() const { return m_project.masterAudioPath; }
    QString vocalsPath() const { return m_project.analysisAudioPath; }
    QString backgroundPath() const { return m_project.backgroundAudioPath; }
    QUrl sourceMediaUrl() const {
        if (m_project.sourceMediaPath.isEmpty()) return QUrl();
        return QUrl::fromLocalFile(m_project.sourceMediaPath);
    }
    QUrl playbackMediaUrl() const;
    QString sourceLanguage() const { return m_project.sourceLanguage; }
    QString targetLanguage() const { return m_project.targetLanguage; }
    QVariantMap durationControl() const { return m_project.durationControl; }
    QVariantList speakers() const { return m_project.speakers; }
    QVariantList segments() const { return m_project.segments; }
    bool processing() const;
    QString stage() const;
    int progress() const;
    QString lastError() const;
    QString previewPath() const;
    QString dubbedVocalPath() const;
    QString exportPath() const;
    QVariantList workflowNodes() const;
    QVariantMap workflowNodeConfigurations() const { return m_workflowNodeConfigurations; }
    bool workflowReady() const;
    QString workflowStatusText() const;
    QString workflowId() const;
    int workflowVersion() const;
    bool workflowGraphValid() const;
    QString workflowRunId() const;
    QString workflowNodeRunId() const;
    bool workflowWaitingForInput() const;
    QVariantMap workflowReviewRequest() const;
    QString workflowMode() const { return m_workflowMode; }
    QString currentStepId() const;
    QVariantMap currentStepOutput() const;
    QString lastCompletedStepId() const { return m_lastCompletedStepId; }
    QVariantList history() const { return m_history; }
    bool translationFixing() const;
    int translationFixProgress() const;
    QString translationFixStatus() const;
    QVariantMap translationFixConfiguration() const;
    int translationFixCandidateCount() const;
    QString dubbingQuality() const { return m_project.dubbingQuality; }
    QString adaptiveProvider() const;
    bool adaptiveReady() const;
    QString adaptiveStatusText() const;
    bool customReady() const;
    QString customStatusText() const;
    bool settingsLocked() const;
    bool automaticSetupActive() const { return m_automaticSetupActive; }
    QString automaticStatusText() const { return m_automaticStatusText; }
    QVariantList automaticEvents() const { return m_automaticEvents; }

    void setSourceLanguage(const QString &value);
    void setTargetLanguage(const QString &value);
    void setDurationControl(const QVariantMap &value);
    void setDubbingQuality(const QString &value);

    Q_INVOKABLE bool newProject(const QString &path = QString());
    Q_INVOKABLE bool openProject(const QString &path);
    Q_INVOKABLE bool saveProject();
    Q_INVOKABLE bool deleteHistoryItem(const QString &id);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE bool importMedia(const QString &pathOrUrl);
    Q_INVOKABLE void transcribeSource();
    Q_INVOKABLE void translateSource();
    Q_INVOKABLE void generateAudio();
    Q_INVOKABLE void cancelProcessing();
    Q_INVOKABLE bool renderPreview(const QString &path = QString());
    Q_INVOKABLE bool exportMedia(const QString &path);
    Q_INVOKABLE bool exportAudioStem(const QString &stem, const QString &path);
    Q_INVOKABLE bool exportSubtitles(const QString &path, bool useTargetText = true);
    Q_INVOKABLE bool exportPackage(const QString &directoryPath);
    Q_INVOKABLE void addSegment(qint64 startMs, qint64 endMs, const QString &sourceText = QString());
    Q_INVOKABLE void updateSegment(int index, const QVariantMap &patch);
    Q_INVOKABLE void removeSegment(int index);
    Q_INVOKABLE void addSpeaker(const QString &name = QString());
    Q_INVOKABLE void setSpeakerVoice(int speakerIndex, const QVariantMap &voice);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void resetStandardWorkflowNodeModels();
    Q_INVOKABLE QString defaultWorkflowModelFamily(const QString &nodeId) const;
    Q_INVOKABLE void prepareWorkflow();
    Q_INVOKABLE bool runWorkflow(const QString &outputPath = QString());
    Q_INVOKABLE bool startAutomaticWorkflow(const QString &outputPath);
    Q_INVOKABLE void pauseAutomaticWorkflow();
    Q_INVOKABLE void startStepByStep();
    Q_INVOKABLE bool runCurrentStep(const QString &outputPath = QString());
    Q_INVOKABLE bool rerunStep(const QString &stepId, const QString &outputPath = QString());
    Q_INVOKABLE QVariantMap stepOutput(const QString &stepId) const;
    Q_INVOKABLE bool approveWorkflowReview(const QVariantMap &artifact = QVariantMap());
    Q_INVOKABLE bool rejectWorkflowReview(const QString &reason = QString());
    Q_INVOKABLE bool setWorkflowNodeModel(const QString &nodeId,
                                          const QString &familyId,
                                          const QString &runtimeId,
                                          const QString &runtimeVersion,
                                          const QVariantMap &selectedFiles = QVariantMap());
    Q_INVOKABLE bool loadWorkflowNodeModel(const QString &nodeId);
    Q_INVOKABLE bool unloadWorkflowNodeModel(const QString &nodeId);
    Q_INVOKABLE bool reloadWorkflowNodeModel(const QString &nodeId);
    Q_INVOKABLE bool setWorkflowNodeParameters(const QString &nodeId, const QVariantMap &parameters);
    Q_INVOKABLE bool fixTranslations(const QVariantMap &configuration = QVariantMap());
    Q_INVOKABLE bool fixTranslationSegment(
        int index, const QVariantMap &configuration = QVariantMap());
    Q_INVOKABLE bool translationSegmentNeedsFix(int index) const;
    Q_INVOKABLE void testTranslationFixConnection(
        const QVariantMap &configuration = QVariantMap());
    Q_INVOKABLE QVariantList translationFixCliModelOptions(
        const QString &cliAgent) const;
    Q_INVOKABLE void cancelTranslationFix();
    Q_INVOKABLE void setAdaptiveConfiguration(const QVariantMap &configuration);

signals:
    void projectChanged();
    void segmentsChanged();
    void processingChanged();
    void errorChanged();
    void previewChanged();
    void exportChanged();
    void workflowChanged();
    void historyChanged();
    void translationFixChanged();
    void translationFixConnectionTested(bool success, const QString &message);
    void workflowSetupRequired(const QString &nodeId, const QString &setupKind,
                               const QString &message);

private slots:
    void onIngestFinished(bool success, const QVariantMap &manifest);

private:
    bool ensureProject(const QString &path);
    void setError(const QString &message);
    void persistAfterEdit();
    void setWorkflowMode(const QString &mode);
    void setCurrentStep(const QString &stepId);
    void advanceManualStep(const QString &completedStepId);
    bool configureWorkflowNodeModel(const QString &nodeId,
                                    const QString &familyId,
                                    const QString &runtimeId,
                                    const QString &runtimeVersion,
                                    const QVariantMap &selectedFiles,
                                    bool loadSession);
    CapabilityFamilyModel *automaticModel(const QString &capabilityId);
    bool ensureAutomaticModel(const QString &nodeId, const QString &capabilityId,
                              bool loadSession);
    bool ensureAutomaticAdaptiveModel();
    QVariantMap firstCustomSetupIssue() const;
    void resetStandardTranslationFixConfiguration();
    void advanceAutomaticSetup();
    void scheduleAutomaticSetupAdvance();
    void prepareAutomaticVoiceRuntime();
    void finishAutomaticSetupFailure(const QString &message);
    void appendAutomaticEvent(const QString &message, const QString &state,
                              const QString &nodeId = QString());
    void setAutomaticStatus(const QString &message);
    static QString visibleStepForNode(const QString &nodeId);
    void loadHistory();
    void recordHistoryEntry();
    QString historyPath() const;

    DubbingProject m_project;
    DubbingJobRunner *m_runner = nullptr;
    NodeRegistry *m_workflowRegistry = nullptr;
    WorkflowGraphRunner *m_workflowRunner = nullptr;
    std::unique_ptr<WorkflowReviewStore> m_workflowReviewStore;
    std::unique_ptr<WorkflowRunJournal> m_workflowJournal;
    QVariantMap m_workflowReviewRequest;
    QString m_activeReviewId;
    QString m_workflowMode = QStringLiteral("idle");
    QString m_currentStepId = QStringLiteral("import");
    QVariantMap m_stepOutputs;
    QString m_lastCompletedStepId;
    QString m_pendingExportPath;
    QVariantList m_history;
    QVariantMap m_workflowNodeConfigurations;
    SttSessionController *m_sttSession = nullptr;
    TtsEngine *m_tts = nullptr;
    TranslationEngine *m_translation = nullptr;
    DubbingTranslationFixService *m_translationFix = nullptr;
    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;
    std::unique_ptr<CapabilityFamilyModel> m_automaticSttModel;
    std::unique_ptr<CapabilityFamilyModel> m_automaticVoiceIsolationModel;
    std::unique_ptr<CapabilityFamilyModel> m_automaticTranslationModel;
    std::unique_ptr<CapabilityFamilyModel> m_automaticTtsModel;
    std::unique_ptr<CapabilityFamilyModel> m_automaticLlmModel;
    bool m_automaticSetupActive = false;
    bool m_automaticAdvanceScheduled = false;
    QString m_automaticOutputPath;
    QString m_automaticStatusText;
    QVariantList m_automaticEvents;
    QSet<QString> m_automaticDownloadsQueued;
    QSet<QString> m_automaticConfiguredNodes;
};

} // namespace LAStudio
