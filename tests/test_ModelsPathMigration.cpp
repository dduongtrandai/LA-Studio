#include "test_ModelsPathMigration.h"
#include <QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QDir>
#include <QThreadPool>
#include <QUrl>

#include "controllers/ModelsPathMigrator.h"
#include "controllers/ModelsPathMigrationService.h"
#include "core/Settings.h"
#include "core/ModelManager.h"
#include "core/PathUtils.h"
#include "core/DownloadManager.h"
#include "core/HFHubClient.h"
#include "stt/SttEngine.h"
#include "tts/TtsEngine.h"

namespace LAStudio {

namespace {

void restoreFile(const QString &path, bool existed, const QByteArray &contents)
{
    if (!existed) {
        QFile::remove(path);
        return;
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(contents);
    }
}

} // namespace

void TestModelsPathMigration::initTestCase()
{
    QVERIFY(m_tempDir.isValid());

    m_settingsJsonPath = PathUtils::dataDir() + QStringLiteral("/settings.json");
    m_hadSettingsJson = QFile::exists(m_settingsJsonPath);
    if (m_hadSettingsJson) {
        QFile file(m_settingsJsonPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        m_settingsJsonContents = file.readAll();
    }

    m_settingsIniPath = PathUtils::dataDir() + QStringLiteral("/settings.ini");
    m_hadSettingsIni = QFile::exists(m_settingsIniPath);
    if (m_hadSettingsIni) {
        QFile file(m_settingsIniPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        m_settingsIniContents = file.readAll();
    }
}

void TestModelsPathMigration::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
    restoreFile(m_settingsJsonPath, m_hadSettingsJson, m_settingsJsonContents);
    restoreFile(m_settingsIniPath, m_hadSettingsIni, m_settingsIniContents);
}

void TestModelsPathMigration::testModelsPathMigrator()
{
    qDebug() << "--- START: testModelsPathMigrator ---";
    QString srcDir = m_tempDir.filePath(QStringLiteral("migrate_src"));
    QString dstDir = m_tempDir.filePath(QStringLiteral("migrate_dst"));

    QDir().mkpath(srcDir);
    QString srcFile = srcDir + QStringLiteral("/model.bin");
    QFile file(srcFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("dummy-data");
    file.close();

    QString errorMsg;
    bool success = ModelsPathMigrator::copyDirectoryMerge(srcDir, dstDir, errorMsg);
    QVERIFY(success);
    QVERIFY(QFile::exists(dstDir + QStringLiteral("/model.bin")));

    // Same size but different content must fail with conflict.
    QString conflictSrcDir = m_tempDir.filePath(QStringLiteral("migrate_conflict_src"));
    QString conflictDstDir = m_tempDir.filePath(QStringLiteral("migrate_conflict_dst"));
    QDir().mkpath(conflictSrcDir);
    QDir().mkpath(conflictDstDir);
    QFile srcConflict(conflictSrcDir + QStringLiteral("/shared.bin"));
    QFile dstConflict(conflictDstDir + QStringLiteral("/shared.bin"));
    QVERIFY(srcConflict.open(QIODevice::WriteOnly));
    QVERIFY(dstConflict.open(QIODevice::WriteOnly));
    srcConflict.write("AAAA");
    dstConflict.write("BBBB");
    srcConflict.close();
    dstConflict.close();

    errorMsg.clear();
    success = ModelsPathMigrator::copyDirectoryMerge(conflictSrcDir, conflictDstDir, errorMsg);
    QVERIFY(!success);
    QVERIFY(errorMsg.contains(QStringLiteral("File conflict"), Qt::CaseInsensitive));

    success = ModelsPathMigrator::removeDirectory(srcDir);
    QVERIFY(success);
    QVERIFY(!QDir(srcDir).exists());
}

void TestModelsPathMigration::testModelsPathMigrationService()
{
    qDebug() << "--- START: testModelsPathMigrationService ---";
    qDebug() << "Step 1: Settings init";
    Settings settings;
    qDebug() << "Step 2: ModelManager init";
    ModelManager models;
    qDebug() << "Step 3: HFHubClient init";
    HFHubClient hub;
    qDebug() << "Step 4: DownloadManager init";
    DownloadManager downloads(&hub);
    qDebug() << "Step 5: SttEngine init";
    SttEngine stt;
    qDebug() << "Step 6: TtsEngine init";
    TtsEngine tts;

    qDebug() << "Step 7: ModelsPathMigrationService init";
    ModelsPathMigrationService service(&settings, &models, &downloads, &stt, &tts);

    // Test identical path rejection
    qDebug() << "Step 8: Get models root";
    QString currentPath = models.modelsRoot();
    qDebug() << "Step 9: QSignalSpy init";
    QSignalSpy spyError(&service, &ModelsPathMigrationService::errorChanged);
    qDebug() << "Step 10: Change directory (same)";
    service.changeDirectory(QUrl::fromLocalFile(currentPath).toString());
    qDebug() << "Step 11: Compare error";
    QCOMPARE(service.error(), QString()); // No error emitted because it returns early silently

    // Test nested path validation error
    qDebug() << "Step 12: Get nested path";
    QString nestedPath = currentPath + QStringLiteral("/nested");
    qDebug() << "Step 13: Change directory (nested)";
    service.changeDirectory(QUrl::fromLocalFile(nestedPath).toString());
    qDebug() << "Step 14: Verify error";
    QVERIFY(!service.error().isEmpty());

    // Cleanup failure should not block commit of new models path.
    const QString oldPath = m_tempDir.filePath(QStringLiteral("service_migrate_old"));
    const QString newPath = m_tempDir.filePath(QStringLiteral("service_migrate_new"));
    QDir().mkpath(oldPath);
    QDir().mkpath(newPath);
    QFile oldModelFile(oldPath + QStringLiteral("/model.bin"));
    QVERIFY(oldModelFile.open(QIODevice::WriteOnly));
    oldModelFile.write("model");
    oldModelFile.close();

    // Keep one file handle open to force cleanup failure on Windows.
    QFile lockFile(oldPath + QStringLiteral("/locked.bin"));
    QVERIFY(lockFile.open(QIODevice::WriteOnly));
    lockFile.write("lock");
    lockFile.flush();

    models.setModelsRoot(oldPath);
    settings.setModelsPath(oldPath);
    service.changeDirectory(QUrl::fromLocalFile(newPath).toString());
    QTRY_VERIFY_WITH_TIMEOUT(!service.running(), 3000);
    QCOMPARE(settings.modelsPath(), QDir(newPath).absolutePath());
    QVERIFY(service.message().contains(QStringLiteral("failed to clean up"), Qt::CaseInsensitive));
    QVERIFY(QDir(oldPath).exists());
    lockFile.close();
    qDebug() << "Step 15: End of testModelsPathMigrationService";
}

} // namespace LAStudio
