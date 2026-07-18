#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QAtomicInteger>
#include <memory>
#include "separation/SeparationTypes.h"

namespace LAStudio {

class SttSessionController;
class TtsEngine;
class ModelManager;
class RuntimeManager;
class TranslationEngine;
class TranslationEngineInstance;
class MediaToolService;
class MediaIngestService;
class SourceSeparationService;

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

    bool processing() const { return m_processing; }
    QString stage() const { return m_stage; }
    int progress() const { return m_progress; }
    QString lastError() const { return m_lastError; }
    QString previewPath() const { return m_previewPath; }
    QString exportPath() const { return m_exportPath; }
    QString runId() const { return m_runId; }
    QString nodeRunId() const { return m_activeNodeRunId; }

    void startIngest(const QString &path);
    void startSourceSeparation(const QString &audioPath,
                               const QVariantMap &modelConfiguration = QVariantMap());
    void startTranscription(const QString &sourceLanguage, const QString &sourceMediaPath);
    void startTranslation(const QString &sourceLanguage, const QString &targetLanguage, const QVariantList &segments,
                          const QVariantMap &modelConfiguration = QVariantMap());
    void startAudioGeneration(const QVariantList &segments, const QString &projectPath);
    void cancel();
    bool renderPreview(const QVariantList &segments, const QString &projectPath, const QString &path = QString());
    bool startExport(const QString &sourceMediaPath, const QString &outputPath);

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
    void onTranscriptionFinished(const QString &text, const QVariantList &segments);
    void onAlignmentFinished();
    void onSynthesisFinished(const QByteArray &pcm16, int sampleRate);
    void onTtsError(const QString &message);
    void onMediaFinished(bool success, const QString &outputPath, const QString &error);
    void onTranslationFinished();
    void onIngestFinished(bool success, const QVariantMap &manifest, const QString &error);
    void onSourceSeparationFinished(const SeparationResult &result);

private:
    void setProcessing(bool value, const QString &stage, int progress);
    void setBusyError(const QString &message);
    void startAlignmentRefinement(const QString &audioPath, const QString &language,
                                  const QVariantList &segments);

    SttSessionController *m_sttSession = nullptr;
    TtsEngine *m_tts = nullptr;
    TranslationEngine *m_translation = nullptr;
    ModelManager *m_models = nullptr;
    RuntimeManager *m_runtimes = nullptr;

    bool m_processing = false;
    QString m_stage;
    int m_progress = 0;
    QString m_lastError;
    QString m_previewPath;
    QString m_exportPath;
    QString m_exportDestination;
    QString m_exportStagingPath;

    int m_generationIndex = -1;
    QVariantList m_activeSegments;
    QString m_projectPath;
    QVariantList m_translationInputSegments;
    QString m_runId;
    QString m_activeNodeRunId;
    QString m_backgroundAudioPath;
    QString m_transcriptionAudioPath;
    QString m_transcriptionLanguage;

    MediaToolService *m_mediaTools = nullptr;
    MediaIngestService *m_mediaIngest = nullptr;
    SourceSeparationService *m_sourceSeparation = nullptr;
    QString m_pendingSourceAudioPath;
    bool m_waitingForTranscriptionInput = false;
    QElapsedTimer m_stageTimer;
    TranslationEngineInstance *m_translationInstance = nullptr;
    QVariantList m_translationResult;
    QMetaObject::Connection m_translationFinishedConnection;
    QMetaObject::Connection m_translationErrorConnection;
    QMetaObject::Connection m_translationLoadedConnection;
    QFutureWatcher<QVariantMap> *m_alignmentWatcher = nullptr;
    std::shared_ptr<QAtomicInteger<bool>> m_alignmentCancel;
    quint64 m_translationGeneration = 0;
};

} // namespace LAStudio
