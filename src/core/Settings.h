#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QtQml/qqml.h>

namespace LAStudio {

class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY deviceChanged)
    Q_PROPERTY(int threads READ threads WRITE setThreads NOTIFY threadsChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString uiLanguage READ uiLanguage WRITE setUiLanguage NOTIFY uiLanguageChanged)
    Q_PROPERTY(QString modelsPath READ modelsPath WRITE setModelsPath NOTIFY modelsPathChanged)
    Q_PROPERTY(QString selectedRuntime READ selectedRuntime WRITE setSelectedRuntime NOTIFY selectedRuntimeChanged)
    Q_PROPERTY(QString selectedTtsRuntime READ selectedTtsRuntime WRITE setSelectedTtsRuntime NOTIFY selectedTtsRuntimeChanged)
    Q_PROPERTY(QString selectedTtsRuntimeVersion READ selectedTtsRuntimeVersion WRITE setSelectedTtsRuntimeVersion NOTIFY selectedTtsRuntimeVersionChanged)
    Q_PROPERTY(QString selectedTtsFamily READ selectedTtsFamily WRITE setSelectedTtsFamily NOTIFY selectedTtsFamilyChanged)
    Q_PROPERTY(QString selectedVoiceCloneFamily READ selectedVoiceCloneFamily WRITE setSelectedVoiceCloneFamily NOTIFY selectedVoiceCloneFamilyChanged)
    Q_PROPERTY(QString selectedSttRuntime READ selectedSttRuntime WRITE setSelectedSttRuntime NOTIFY selectedSttRuntimeChanged)
    Q_PROPERTY(QString selectedSttRuntimeVersion READ selectedSttRuntimeVersion WRITE setSelectedSttRuntimeVersion NOTIFY selectedSttRuntimeVersionChanged)
    Q_PROPERTY(QString selectedSttFamily READ selectedSttFamily WRITE setSelectedSttFamily NOTIFY selectedSttFamilyChanged)
    Q_PROPERTY(QString selectedSttModelPath READ selectedSttModelPath WRITE setSelectedSttModelPath NOTIFY selectedSttModelPathChanged)
    Q_PROPERTY(QString selectedSttModelFile READ selectedSttModelFile WRITE setSelectedSttModelFile NOTIFY selectedSttModelFileChanged)
    Q_PROPERTY(QString sttLanguage READ sttLanguage WRITE setSttLanguage NOTIFY sttLanguageChanged)
    Q_PROPERTY(int sttThreads READ sttThreads WRITE setSttThreads NOTIFY sttThreadsChanged)
    Q_PROPERTY(bool sttTranslate READ sttTranslate WRITE setSttTranslate NOTIFY sttTranslateChanged)
    Q_PROPERTY(bool offloadKvCache READ offloadKvCache WRITE setOffloadKvCache NOTIFY offloadKvCacheChanged)
    Q_PROPERTY(int guardrailMode READ guardrailMode WRITE setGuardrailMode NOTIFY guardrailModeChanged)
    Q_PROPERTY(bool apiServerEnabled READ apiServerEnabled WRITE setApiServerEnabled NOTIFY apiServerEnabledChanged)
    Q_PROPERTY(bool apiServerAllowLan READ apiServerAllowLan WRITE setApiServerAllowLan NOTIFY apiServerAllowLanChanged)
    Q_PROPERTY(int apiServerPort READ apiServerPort WRITE setApiServerPort NOTIFY apiServerPortChanged)
    Q_PROPERTY(QString apiServerApiKey READ apiServerApiKey WRITE setApiServerApiKey NOTIFY apiServerApiKeyChanged)


public:
    explicit Settings(QObject *parent = nullptr);

    QString device() const;
    void setDevice(const QString &v);

    int threads() const;
    void setThreads(int v);

    QString language() const;
    void setLanguage(const QString &v);

    QString uiLanguage() const;
    void setUiLanguage(const QString &v);

    QString modelsPath() const;
    void setModelsPath(const QString &v);

    QString selectedRuntime() const;
    void setSelectedRuntime(const QString &v);

    QString selectedTtsRuntime() const;
    void setSelectedTtsRuntime(const QString &v);

    QString selectedTtsRuntimeVersion() const;
    void setSelectedTtsRuntimeVersion(const QString &v);

    QString selectedTtsFamily() const;
    void setSelectedTtsFamily(const QString &v);

    QString selectedVoiceCloneFamily() const;
    void setSelectedVoiceCloneFamily(const QString &v);

    QString selectedSttRuntime() const;
    void setSelectedSttRuntime(const QString &v);

    QString selectedSttRuntimeVersion() const;
    void setSelectedSttRuntimeVersion(const QString &v);

    QString selectedSttFamily() const;
    void setSelectedSttFamily(const QString &v);

    QString selectedSttModelPath() const;
    void setSelectedSttModelPath(const QString &v);

    QString selectedSttModelFile() const;
    void setSelectedSttModelFile(const QString &v);

    QString sttLanguage() const;
    void setSttLanguage(const QString &v);

    int sttThreads() const;
    void setSttThreads(int v);

    bool sttTranslate() const;
    void setSttTranslate(bool v);

    bool offloadKvCache() const;
    void setOffloadKvCache(bool v);

    int guardrailMode() const;
    void setGuardrailMode(int v);

    bool apiServerEnabled() const;
    void setApiServerEnabled(bool v);

    bool apiServerAllowLan() const;
    void setApiServerAllowLan(bool v);

    int apiServerPort() const;
    void setApiServerPort(int v);

    QString apiServerApiKey() const;
    void setApiServerApiKey(const QString &v);


signals:
    void deviceChanged();
    void threadsChanged();
    void languageChanged();
    void uiLanguageChanged();
    void modelsPathChanged();
    void selectedRuntimeChanged();
    void selectedTtsRuntimeChanged();
    void selectedTtsRuntimeVersionChanged();
    void selectedTtsFamilyChanged();
    void selectedVoiceCloneFamilyChanged();
    void selectedSttRuntimeChanged();
    void selectedSttRuntimeVersionChanged();
    void selectedSttFamilyChanged();
    void selectedSttModelPathChanged();
    void selectedSttModelFileChanged();
    void sttLanguageChanged();
    void sttThreadsChanged();
    void sttTranslateChanged();
    void offloadKvCacheChanged();
    void guardrailModeChanged();
    void apiServerEnabledChanged();
    void apiServerAllowLanChanged();
    void apiServerPortChanged();
    void apiServerApiKeyChanged();


private:
    QSettings m_settings;
    QString m_device;
    int m_threads;
    QString m_language;
    QString m_uiLanguage;
    QString m_modelsPath;
    QString m_selectedRuntime;
    QString m_selectedTtsRuntime;
    QString m_selectedTtsRuntimeVersion;
    QString m_selectedTtsFamily;
    QString m_selectedVoiceCloneFamily;
    QString m_selectedSttRuntime;
    QString m_selectedSttRuntimeVersion;
    QString m_selectedSttFamily;
    QString m_selectedSttModelPath;
    QString m_selectedSttModelFile;
    QString m_sttLanguage;
    int m_sttThreads;
    bool m_sttTranslate;
    bool m_offloadKvCache;
    int m_guardrailMode;
    bool m_apiServerEnabled;
    bool m_apiServerAllowLan;
    int m_apiServerPort;
    QString m_apiServerApiKey;
};


} // namespace LAStudio

