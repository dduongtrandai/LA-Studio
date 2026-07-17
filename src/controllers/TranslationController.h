#pragma once

#include <QObject>
#include <QTimer>
#include <QtQml/qqml.h>
#include <atomic>
#include <memory>

#include "translation/TranslationProject.h"
#include "translation/TranslationEngine.h"

namespace LAStudio {
class TranslationModelSession;
class TranslationController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("TranslationController is managed by AppController")
    Q_PROPERTY(QVariantList segments READ segments NOTIFY projectChanged)
    Q_PROPERTY(QString sourceLanguage READ sourceLanguage WRITE setSourceLanguage NOTIFY projectChanged)
    Q_PROPERTY(QString targetLanguage READ targetLanguage WRITE setTargetLanguage NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(QString sourceFormat READ sourceFormat NOTIFY projectChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY projectChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)
    Q_PROPERTY(QString activeSegmentId READ activeSegmentId NOTIFY processingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY processingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY processingChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
public:
    TranslationController(TranslationEngine *engine, TranslationModelSession *session, QObject *parent = nullptr);
    QVariantList segments() const { return m_project.segments; } QString sourceLanguage() const { return m_project.sourceLanguage; } QString targetLanguage() const { return m_project.targetLanguage; } QString projectPath() const { return m_project.projectPath; } QString sourceFormat() const { return m_project.sourceFormat; } bool dirty() const { return m_dirty; } bool processing() const { return m_processing; } QString activeSegmentId() const { return m_activeSegmentId; } int progress() const { return m_progress; } QString statusText() const; QString errorText() const { return m_error; } QVariantList history() const { return m_history; }
    void setSourceLanguage(const QString &value); void setTargetLanguage(const QString &value);
    Q_INVOKABLE void newProject(); Q_INVOKABLE bool openProject(const QString &path); Q_INVOKABLE bool importText(const QString &text); Q_INVOKABLE bool importFile(const QString &path); Q_INVOKABLE bool saveProject(); Q_INVOKABLE bool saveProjectAs(const QString &path); Q_INVOKABLE bool exportResult(const QString &path); Q_INVOKABLE void updateSegment(int index, const QVariantMap &patch); Q_INVOKABLE void removeSegment(int index); Q_INVOKABLE void addSegment(); Q_INVOKABLE void swapLanguages(); Q_INVOKABLE void translateAll(); Q_INVOKABLE void translateSegment(int index); Q_INVOKABLE void cancel(); Q_INVOKABLE bool loadHistoryItem(const QString &id); Q_INVOKABLE bool deleteHistoryItem(const QString &id); Q_INVOKABLE void clearHistory();
signals: void projectChanged(); void processingChanged(); void errorTextChanged(); void historyChanged();
private: void markDirty(); void setError(const QString &message); void startTranslation(const QVariantList &segments, const QString &activeSegmentId = QString()); void applyPatches(const QVariantList &patches); void autosave(); QString historyPath() const; void loadHistory(); void addHistory();
    TranslationEngine *m_engine = nullptr; TranslationModelSession *m_session = nullptr; TranslationProject m_project; QTimer m_autosave; std::shared_ptr<std::atomic_bool> m_cancelToken; bool m_dirty = false; bool m_processing = false; QString m_activeSegmentId; int m_progress = 0; QString m_error; QVariantList m_history;
};
} // namespace LAStudio
