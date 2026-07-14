#include "test_DownloadInstallService.h"
#include <QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QThreadPool>

#include "controllers/DownloadInstallService.h"
#include "core/HFHubClient.h"
#include "core/DownloadManager.h"
#include "core/ModelManager.h"
#include "core/Settings.h"
#include "core/RuntimeManager.h"

namespace LAStudio {

void TestDownloadInstallService::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestDownloadInstallService::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
}

void TestDownloadInstallService::testDownloadInstallService()
{
    qDebug() << "--- START: testDownloadInstallService ---";
    HFHubClient hub;
    DownloadManager downloads(&hub);
    ModelManager models;
    Settings settings;
    RuntimeManager runtimes(nullptr, &settings);

    DownloadInstallService service(&downloads, &models, &runtimes);

    // Create a dummy model zip with incorrect signature to verify rejection
    QString dummyZip = m_tempDir.filePath(QStringLiteral("bad.zip"));
    QFile file(dummyZip);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("NOT-A-ZIP-HEADER");
    file.close();

    QSignalSpy spyError(&service, &DownloadInstallService::errorOccurred);
    
    // Simulate finished download event
    emit downloads.finished(QStringLiteral("test-model"), QStringLiteral("bad.zip"), dummyZip, {});

    QVERIFY(spyError.size() > 0 || !QFile::exists(dummyZip)); // Either error fired or bad file deleted
}

void TestDownloadInstallService::testQuickInstallSelectsLatestCatalogRuntime()
{
    QVariantMap oldRuntime{
        {QStringLiteral("id"), QStringLiteral("example-cpu")},
        {QStringLiteral("version"), QStringLiteral("v1.9.0")},
        {QStringLiteral("asset"), QStringLiteral("old.zip")}
    };
    QVariantMap latestRuntime{
        {QStringLiteral("id"), QStringLiteral("example-cpu")},
        {QStringLiteral("version"), QStringLiteral("v1.10.0")},
        {QStringLiteral("asset"), QStringLiteral("latest.zip")}
    };
    QVariantMap option = oldRuntime;
    option.insert(QStringLiteral("latestVersion"), QStringLiteral("v1.10.0"));
    option.insert(QStringLiteral("versionOptions"), QVariantList{latestRuntime, oldRuntime});

    const QVariantMap selected = DownloadInstallService::latestSupportedRuntime(option);
    QCOMPARE(selected.value(QStringLiteral("version")).toString(), QStringLiteral("v1.10.0"));
    QCOMPARE(selected.value(QStringLiteral("asset")).toString(), QStringLiteral("latest.zip"));
}

} // namespace LAStudio
