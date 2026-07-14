#include "test_FileAccessService.h"
#include <QtTest>
#include <QFile>
#include <QTextStream>
#include "controllers/FileAccessService.h"

namespace LAStudio {

void TestFileAccessService::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestFileAccessService::testFileAccessService()
{
    qDebug() << "--- START: testFileAccessService ---";
    FileAccessService service;

    // Test urlToLocalPath
    QString urlStr = QStringLiteral("file:///test/path/file.txt");
    QString resolved = service.urlToLocalPath(urlStr);
    QVERIFY(!resolved.isEmpty());

    // Test readTextFile and localFileExists
    QString tempFilePath = m_tempDir.filePath(QStringLiteral("test_file.txt"));
    QFile file(tempFilePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);
    out << QStringLiteral("Hello World");
    file.close();

    QVERIFY(service.localFileExists(tempFilePath));
    QString content = service.readTextFile(tempFilePath);
    QCOMPARE(content, QStringLiteral("Hello World"));
}

} // namespace LAStudio
