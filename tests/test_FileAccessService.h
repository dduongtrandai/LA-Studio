#pragma once
#include <QObject>
#include <QTemporaryDir>

namespace LAStudio {

class TestFileAccessService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testFileAccessService();

private:
    QTemporaryDir m_tempDir;
};

} // namespace LAStudio
