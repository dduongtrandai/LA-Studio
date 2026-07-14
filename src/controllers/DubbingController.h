#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>
#include <memory>

#include "DubbingProject.h"
#include "workflows/NodeRegistry.h"
#include "workflows/WorkflowGraphRunner.h"

namespace LAStudio {

class SttSessionController;
class TtsEngine;
class ModelManager;
class RuntimeManager;
class DubbingJobRunner;

class DubbingController : public QObject
{
    Q_OBJECT
    QML_UNCREATABLE("DubbingController is managed by AppController")

    Q_PROPERTY(bool hasProject READ hasProject NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(QString sourceMediaPath READ sourceMediaPath NOTIFY projectChanged)
    Q_PROPERTY(QUrl sourceMediaUrl READ sourceMediaUrl NOTIFY projectChanged)
    Q_PROPERTY(QString sourceLanguage READ sourceLanguage WRITE setSourceLanguage NOTIFY projectChanged)
    Q_PROPERTY(QString targetLanguage READ targetLanguage WRITE setTargetLanguage NOTIFY projectChanged)
    Q_PROPERTY(QVariantList speakers READ speakers NOTIFY projectChanged)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY segmentsChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY processingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
    Q_PROPERTY(QString previewPath READ previewPath NOTIFY previewChanged)
    Q_PROPERTY(QString exportPath READ exportPath NOTIFY exportChanged)
    Q_PROPERTY(QVariantList workflowNodes READ workflowNodes NOTIFY workflowChanged)
    Q_PROPERTY(bool workflowReady READ workflowReady NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowStatusText READ workflowStatusText NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowId READ workflowId CONSTANT)
    Q_PROPERTY(int workflowVersion READ workflowVersion CONSTANT)
    Q_PROPERTY(bool workflowGraphValid READ workflowGraphValid NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowRunId READ workflowRunId NOTIFY processingChanged)
    Q_PROPERTY(QString workflowNodeRunId READ workflowNodeRunId NOTIFY processingChanged)
    Q_PROPERTY(bool workflowWaitingForInput READ workflowWaitingForInput NOTIFY workflowChanged)
    Q_PROPERTY(QVariantMap workflowReviewRequest READ workflowReviewRequest NOTIFY workflowChanged)

public:
    explicit DubbingController(SttSessionController *sttSession, TtsEngine *tts,
                               ModelManager *models = nullptr, RuntimeManager *runtimes = nullptr,
                               QObject *parent = nullptr);

    bool hasProject() const { return !m_project.projectPath.isEmpty(); }
    QString projectPath() const { return m_project.projectPath; }
    QString sourceMediaPath() const { return m_project.sourceMediaPath; }
    QUrl sourceMediaUrl() const {
        if (m_project.sourceMediaPath.isEmpty()) return QUrl();
        return QUrl::fromLocalFile(m_project.sourceMediaPath);
    }
    QString sourceLanguage() const { return m_project.sourceLanguage; }
    QString targetLanguage() const { return m_project.targetLanguage; }
    QVariantList speakers() const { return m_project.speakers; }
    QVariantList segments() const { return m_project.segments; }
    bool processing() const;
    QString stage() const;
    int progress() const;
    QString lastError() const;
    QString previewPath() const;
    QString exportPath() const;
    QVariantList workflowNodes() const;
    bool workflowReady() const;
    QString workflowStatusText() const;
    QString workflowId() const;
    int workflowVersion() const;
    bool workflowGraphValid() const;
    QString workflowRunId() const;
    QString workflowNodeRunId() const;
    bool workflowWaitingForInput() const;
    QVariantMap workflowReviewRequest() const;

    void setSourceLanguage(const QString &value);
    void setTargetLanguage(const QString &value);

    Q_INVOKABLE bool newProject(const QString &path = QString());
    Q_INVOKABLE bool openProject(const QString &path);
    Q_INVOKABLE bool saveProject();
    Q_INVOKABLE void closeProject();
    Q_INVOKABLE bool importMedia(const QString &pathOrUrl);
    Q_INVOKABLE void transcribeSource();
    Q_INVOKABLE void translateSource();
    Q_INVOKABLE void generateAudio();
    Q_INVOKABLE void cancelProcessing();
    Q_INVOKABLE bool renderPreview(const QString &path = QString());
    Q_INVOKABLE bool exportMedia(const QString &path);
    Q_INVOKABLE void addSegment(qint64 startMs, qint64 endMs, const QString &sourceText = QString());
    Q_INVOKABLE void updateSegment(int index, const QVariantMap &patch);
    Q_INVOKABLE void removeSegment(int index);
    Q_INVOKABLE void addSpeaker(const QString &name = QString());
    Q_INVOKABLE void setSpeakerVoice(int speakerIndex, const QVariantMap &voice);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void prepareWorkflow();
    Q_INVOKABLE bool runWorkflow(const QString &outputPath = QString());
    Q_INVOKABLE bool approveWorkflowReview(const QVariantMap &artifact = QVariantMap());
    Q_INVOKABLE bool rejectWorkflowReview(const QString &reason = QString());

signals:
    void projectChanged();
    void segmentsChanged();
    void processingChanged();
    void errorChanged();
    void previewChanged();
    void exportChanged();
    void workflowChanged();

private slots:
    void onIngestFinished(bool success, const QVariantMap &manifest);

private:
    bool ensureProject(const QString &path);
    void setError(const QString &message);
    void persistAfterEdit();

    DubbingProject m_project;
    DubbingJobRunner *m_runner = nullptr;
    NodeRegistry *m_workflowRegistry = nullptr;
    WorkflowGraphRunner *m_workflowRunner = nullptr;
    std::unique_ptr<WorkflowReviewStore> m_workflowReviewStore;
    std::unique_ptr<WorkflowRunJournal> m_workflowJournal;
    QVariantMap m_workflowReviewRequest;
    QString m_activeReviewId;
    SttSessionController *m_sttSession = nullptr;
    TtsEngine *m_tts = nullptr;
};

} // namespace LAStudio
