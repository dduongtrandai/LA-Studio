#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QFutureWatcher>
#include <QAtomicInteger>
#include <memory>
#include <QPointer>
#include "separation/SeparationTypes.h"
#include "controllers/dubbing/DubbingRunCoordinator.h"

namespace LAStudio {

class SttSessionController;
class TtsEngine;
class ModelManager;
class RuntimeManager;
class TranslationEngine;
class MediaToolService;
class MediaIngestService;
class SourceSeparationService;
class DubbingTranscriptionJob;
class DubbingSynthesisJob;
class DubbingExportJob;
class DubbingTranslationJob;
class DubbingTranslationFixService;

class DubbingJobRunner : public QObject
{
    Q_OBJECT
public:
    explicit DubbingJobRunner(SttSessionController *sttSession, TtsEngine *tts,
                              TranslationEngine *translation,
                              ModelManager *models = nullptr, RuntimeManager *runtimes = nullptr,
                              QObject *parent = nullptr);
    DubbingJobRunner(SttSessionController *sttSession, TtsEngine *tts,
                     ModelManager *models = nullptr, RuntimeManager *runtimes = nullptr,
                     QObject *parent = nullptr);
    ~DubbingJobRunner() override;

    bool processing() const { return m_run.processing(); }
    QString stage() const { return m_run.stageName(); }
    int progress() const { return m_run.progress(); }
    QString lastError() const { return m_run.lastError(); }
    QString previewPath() const { return m_previewPath; }
    QString exportPath() const { return m_exportPath; }
    QString runId() const { return m_run.runId(); }
    QString nodeRunId() const { return m_run.nodeRunId(); }

    void startIngest(const QString &path);
    void startSourceSeparation(const QString &audioPath,
                               const QVariantMap &modelConfiguration = QVariantMap());
    void startTranscription(const QString &sourceLanguage, const QString &sourceMediaPath);
    void startTranslation(const QString &sourceLanguage, const QString &targetLanguage, const QVariantList &segments,
                          const QVariantMap &modelConfiguration = QVariantMap());
    void setTranslationFixConfiguration(const QVariantMap &configuration);
    void startAudioGeneration(const QVariantList &segments, const QString &projectPath,
                              const QVariantMap &synthesisSettings = QVariantMap());
    void fitTiming(const QVariantList &segments, const QString &projectPath);
    void cancel();
    bool renderPreview(const QVariantList &segments, const QString &projectPath, const QString &path = QString());
    bool startExport(const QString &sourceMediaPath, const QString &outputPath);
    bool startExport(const QString &sourceMediaPath, const QString &audioPath,
                     const QString &outputPath, const QVariantList &segments = {});

    // Helpers to let controller update/clear state in runner
    void setPreviewPath(const QString &path);
    void setExportPath(const QString &path);
    void clearError();
    void setProcessingState(bool value, const QString &stage, int progress);
    void setError(const QString &message);
    void setBackgroundAudioPath(const QString &path) { m_backgroundAudioPath = path; }

signals:
    void stateChanged();
    void segmentsUpdated(const QVariantList &segments);
    void segmentUpdated(int index, const QVariantMap &patch);
    void errorOccurred(const QString &message);
    void ingestFinished(bool success, const QVariantMap &manifest);
    void sourceSeparationFinished(const QVariantMap &outputs);
    void stageCompleted(const QString &nodeId, const QVariantMap &outputs);

private slots:
    void onIngestFinished(bool success, const QVariantMap &manifest, const QString &error);
    void onSourceSeparationFinished(const SeparationResult &result);
    void onTimingFinished();

private:
    void setProcessing(bool value, const QString &stage, int progress);
    void setBusyError(const QString &message);
    void finishTranslation(const QVariantList &segments);

    QPointer<SttSessionController> m_sttSession;
    QPointer<TtsEngine> m_tts;
    QPointer<TranslationEngine> m_translation;
    QPointer<ModelManager> m_models;
    QPointer<RuntimeManager> m_runtimes;

    DubbingRunCoordinator m_run;
    QString m_previewPath;
    QString m_exportPath;

    QVariantList m_activeSegments;
    QString m_projectPath;
    QString m_backgroundAudioPath;

    MediaIngestService *m_mediaIngest = nullptr;
    SourceSeparationService *m_sourceSeparation = nullptr;
    DubbingTranscriptionJob *m_transcriptionJob = nullptr;
    DubbingSynthesisJob *m_synthesisJob = nullptr;
    DubbingExportJob *m_exportJob = nullptr;
    DubbingTranslationJob *m_translationJob = nullptr;
    DubbingTranslationFixService *m_autoTranslationFix = nullptr;
    QVariantMap m_translationConfiguration;
    QVariantMap m_translationFixConfiguration;
    QString m_translationSourceLanguage;
    QString m_translationTargetLanguage;
    QString m_pendingSourceAudioPath;
    QFutureWatcher<QVariantList> *m_timingWatcher = nullptr;
    std::shared_ptr<QAtomicInteger<bool>> m_timingCancel;
};

} // namespace LAStudio
