#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqml.h>
#include <QTemporaryDir>
#include <memory>

#include "separation/SourceSeparationService.h"
#include "separation/SeparationTypes.h"

namespace LAStudio {

class VoiceIsolatorController final : public QObject
{
    Q_OBJECT
    QML_UNCREATABLE("VoiceIsolatorController is managed by AppController")
    Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath NOTIFY stateChanged)
    Q_PROPERTY(QString runtimePath READ runtimePath WRITE setRuntimePath NOTIFY stateChanged)
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY stateChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY stateChanged)
    Q_PROPERTY(int progress READ progress NOTIFY stateChanged)
    Q_PROPERTY(QString vocalsPath READ vocalsPath NOTIFY stateChanged)
    Q_PROPERTY(QString backgroundPath READ backgroundPath NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(int threadCount READ threadCount WRITE setThreadCount NOTIFY stateChanged)
    Q_PROPERTY(QVariantList recentResults READ recentResults NOTIFY stateChanged)
    Q_PROPERTY(QVariantList vocalsSamples READ vocalsSamples NOTIFY vocalsSamplesChanged)
    Q_PROPERTY(QVariantList backgroundSamples READ backgroundSamples NOTIFY backgroundSamplesChanged)
public:
    explicit VoiceIsolatorController(QObject *parent = nullptr);
    QVariantList vocalsSamples() const { return m_vocalsSamples; }
    QVariantList backgroundSamples() const { return m_backgroundSamples; }
    QString sourcePath() const { return m_sourcePath; }
    QString runtimePath() const;
    QString modelPath() const;
    QString spleeterVocalsPath() const;
    QString spleeterAccompanimentPath() const;
    bool processing() const { return m_processing; }
    int progress() const { return m_progress; }
    QString vocalsPath() const { return m_vocalsPath; }
    QString backgroundPath() const { return m_backgroundPath; }
    QString lastError() const { return m_lastError; }
    QString warning() const { return m_warning; }
    bool ready() const;
    int threadCount() const { return m_threadCount; }
    QVariantList recentResults() const { return m_recentResults; }

    void setSourcePath(const QString &path);
    void setRuntimePath(const QString &path);
    void setModelPath(const QString &path);
    void setThreadCount(int value);
    void applyModelConfiguration(const QString &runtimePath, const QVariantMap &resolvedPaths);
    void applySeparationConfiguration(const SeparationConfiguration &config);
    void clearModelConfiguration();
    Q_INVOKABLE void isolate(bool fast = false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearResult();
    Q_INVOKABLE bool exportStem(const QString &sourcePath, const QString &destinationPath);
    Q_INVOKABLE void openRecent(const QString &vocalsPath, const QString &backgroundPath);

signals:
    void stateChanged();
    void vocalsSamplesChanged();
    void backgroundSamplesChanged();

private:
    void loadVocalsSamples(const QString &path);
    void loadBackgroundSamples(const QString &path);
    void addRecent(const QVariantMap &result);
    SourceSeparationService *m_service = nullptr;
    QString m_sourcePath;
    QString m_runtimePath;
    QString m_modelPath;
    QString m_spleeterVocalsPath;
    QString m_spleeterAccompanimentPath;
    bool m_configurationOverrideActive = false;
    SeparationConfiguration m_activeConfig;
    bool m_hasActiveConfig = false;
    bool m_processing = false;
    int m_progress = 0;
    QString m_vocalsPath;
    QString m_backgroundPath;
    QVariantList m_vocalsSamples;
    QVariantList m_backgroundSamples;
    QString m_lastError;
    QString m_warning;
    QVariantList m_recentResults;
    int m_threadCount = 1;
    std::unique_ptr<QTemporaryDir> m_tempDir;
};
} // namespace LAStudio
