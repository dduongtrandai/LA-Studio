#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QTemporaryDir>

namespace LAStudio {

class TestModelsPathMigration : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testModelsPathMigrator();
    void testModelsPathMigrationService();

private:
    QTemporaryDir m_tempDir;
    QString m_settingsJsonPath;
    QByteArray m_settingsJsonContents;
    bool m_hadSettingsJson = false;
    QString m_settingsIniPath;
    QByteArray m_settingsIniContents;
    bool m_hadSettingsIni = false;
};

} // namespace LAStudio
