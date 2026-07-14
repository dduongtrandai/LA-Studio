#pragma once
#include <QObject>
#include <QTemporaryDir>

namespace LAStudio {

class TestAudioPreviewService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testAudioPreviewService();

private:
    QTemporaryDir m_tempDir;
};

} // namespace LAStudio
