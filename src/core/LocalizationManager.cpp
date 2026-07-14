#include "LocalizationManager.h"
#include "Settings.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QVariantMap>
#include <QDebug>

namespace LAStudio {

LocalizationManager::LocalizationManager(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    m_currentLanguage = m_settings->uiLanguage();
    
    connect(m_settings, &Settings::uiLanguageChanged, this, [this]() {
        loadLanguage(m_settings->uiLanguage());
    });
}

QString LocalizationManager::currentLanguage() const
{
    return m_currentLanguage;
}

int LocalizationManager::revision() const
{
    return m_revision;
}

void LocalizationManager::setCurrentLanguage(const QString &lang)
{
    if (m_currentLanguage != lang) {
        m_settings->setUiLanguage(lang);
    }
}

QVariantList LocalizationManager::availableLanguages() const
{
    QVariantList list;

    QVariantMap en;
    en[QStringLiteral("value")] = QStringLiteral("en");
    en[QStringLiteral("text")] = QStringLiteral("English");
    list.append(en);

    QVariantMap vi;
    vi[QStringLiteral("value")] = QStringLiteral("vi");
    vi[QStringLiteral("text")] = QStringLiteral("Tiếng Việt");
    list.append(vi);

    return list;
}

void LocalizationManager::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
    loadLanguage(m_currentLanguage);
}

void LocalizationManager::loadLanguage(const QString &lang)
{
    QCoreApplication::removeTranslator(&m_translator);

    if (lang == QStringLiteral("en")) {
        m_currentLanguage = lang;
        ++m_revision;
        emit currentLanguageChanged();
        emit revisionChanged();
        if (m_engine) {
            m_engine->retranslate();
        }
        Logger::info(QStringLiteral("Localization"), QStringLiteral("Language set to English"));
        return;
    }

    if (m_translator.load(QStringLiteral("lastudio_%1").arg(lang), QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(&m_translator);
        m_currentLanguage = lang;
        Logger::info(QStringLiteral("Localization"), QStringLiteral("Loaded translation for %1").arg(lang));
    } else {
        Logger::error(QStringLiteral("Localization"), QStringLiteral("Failed to load translation for %1").arg(lang));
        m_currentLanguage = QStringLiteral("en");
    }

    ++m_revision;
    emit currentLanguageChanged();
    emit revisionChanged();

    if (m_engine) {
        m_engine->retranslate();
    }
}

} // namespace LAStudio
