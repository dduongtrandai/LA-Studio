#pragma once

#include "dubbing/DubbingDuration.h"
#include "translation/backends/TranslationBackend.h"
#include "translation/TranslationService.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

namespace LAStudio {

class ModelManager;
class RuntimeManager;
class TtsEngine;
class TranslationEngine;
class TranslationEngineInstance;

class DubbingTranslationJob final : public QObject
{
    Q_OBJECT
public:
    DubbingTranslationJob(TranslationEngine *translation, ModelManager *models,
                          RuntimeManager *runtimes, TtsEngine *tts,
                          QObject *parent = nullptr);

    bool running() const { return m_running; }
    bool start(const QString &sourceLanguage, const QString &targetLanguage,
               const QVariantList &segments, const QVariantMap &configuration,
               const QString &runId);
    void cancel();

signals:
    void progressChanged(int progress);
    void completed(const QVariantList &segments);
    void failed(const QString &message);

private:
    void onTranslationFinished(const QVariantList &patches, quint64 generation);
    void requestDurationCandidates();
    void requestPauseAlignment();
    void finishDurationTranslation();
    void fail(const QString &message);
    void setProgressForWorker();
    bool prepareRequest(const QString &sourceLanguage, const QString &targetLanguage,
                        const QVariantMap &configuration, TranslationRequest &request,
                        QString *error) const;

    QPointer<TranslationEngine> m_translation;
    QPointer<ModelManager> m_models;
    QPointer<RuntimeManager> m_runtimes;
    QPointer<TtsEngine> m_tts;
    QPointer<TranslationEngineInstance> m_instance;
    QVariantList m_inputSegments;
    QVariantList m_result;
    TranslationInferenceRequest m_pendingRequest;
    DubbingDurationSettings m_durationSettings;
    double m_durationRate = 10.0;
    int m_durationIteration = 0;
    bool m_durationAware = false;
    bool m_running = false;
    QString m_phase;
    QString m_sourceLanguage;
    QString m_targetLanguage;
    QString m_runId;
    quint64 m_generation = 0;
    QMetaObject::Connection m_finishedConnection;
    QMetaObject::Connection m_errorConnection;
    QMetaObject::Connection m_loadedConnection;
    QMetaObject::Connection m_progressConnection;
};

} // namespace LAStudio
