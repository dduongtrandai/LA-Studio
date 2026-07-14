#pragma once
#include <QObject>
#include <QTemporaryDir>

namespace LAStudio {

class TestHistory : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testHistoryRepository();
    void testHistoryService();
    void testSttHistory();

private:
    QTemporaryDir m_tempDir;
};

} // namespace LAStudio
