#include "test_AudioPreviewService.h"
#include <QtTest>
#include <QSignalSpy>
#include <QThreadPool>

#include "controllers/AudioPreviewService.h"
#include "tts/TtsEngine.h"
#include "audio/AudioPlayer.h"
#include "audio/WaveformProvider.h"

namespace LAStudio {

void TestAudioPreviewService::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestAudioPreviewService::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
}

void TestAudioPreviewService::testAudioPreviewService()
{
    qDebug() << "--- START: testAudioPreviewService ---";
    TtsEngine tts;
    AudioPlayer player;
    WaveformProvider provider;

    AudioPreviewService service(&tts, &player, &provider);

    QSignalSpy spyError(&service, &AudioPreviewService::errorOccurred);

    // Save with no samples should trigger error
    tts.clearLastSamples();
    service.saveWav(m_tempDir.filePath(QStringLiteral("output.wav")));
    
    // Wait for async worker or check synchronous validation
    if (spyError.isEmpty()) {
        spyError.wait(1000);
    }
    QVERIFY(spyError.size() > 0);
}

} // namespace LAStudio
