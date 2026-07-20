#pragma once
#include <QObject>
#include <QTemporaryDir>

namespace LAStudio {

class TestSourceSeparation : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testBackendFactory();
    void testSharedAudioDecoderNormalizesReferenceAudio();
    void testServiceReentryBusy();
    void testCancellation();
    void testDestroyServiceRunning();

private:
    QTemporaryDir m_tempDir;
    QString m_testWavPath;
};

} // namespace LAStudio
