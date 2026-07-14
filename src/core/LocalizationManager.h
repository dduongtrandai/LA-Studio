#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QTranslator>
#include <QtQml/qqml.h>

class QQmlEngine;

namespace LAStudio {

class Settings;

class LocalizationManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("LocalizationManager is created by AppController")

    Q_PROPERTY(QString currentLanguage READ currentLanguage WRITE setCurrentLanguage NOTIFY currentLanguageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit LocalizationManager(Settings *settings, QObject *parent = nullptr);

    QString currentLanguage() const;
    void setCurrentLanguage(const QString &lang);
    int revision() const;

    QVariantList availableLanguages() const;

    void setEngine(QQmlEngine *engine);

signals:
    void currentLanguageChanged();
    void revisionChanged();

private:
    void loadLanguage(const QString &lang);

    Settings *m_settings = nullptr;
    QQmlEngine *m_engine = nullptr;
    QTranslator m_translator;
    QString m_currentLanguage;
    int m_revision = 0;
};

} // namespace LAStudio
