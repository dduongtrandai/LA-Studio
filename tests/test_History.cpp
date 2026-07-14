#include "test_History.h"
#include <QtTest>
#include <QSignalSpy>
#include <QThreadPool>

#include "controllers/HistoryRepository.h"
#include "controllers/HistoryService.h"
#include "tts/TtsEngine.h"
#include "audio/AudioRecorder.h"

namespace LAStudio {

void TestHistory::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void TestHistory::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
}

void TestHistory::testHistoryRepository()
{
    qDebug() << "--- START: testHistoryRepository ---";
    QString dataDir = m_tempDir.path();

    // TTS Save
    QVector<float> samples = {0.5f, -0.5f, 0.2f, -0.2f};
    int sampleRate = 16000;
    QString errorMsg;
    bool ok = HistoryRepository::addTtsHistoryItem(dataDir, QStringLiteral("hello"), QStringLiteral("model"), QStringLiteral("voice"), samples, sampleRate, errorMsg);
    QVERIFY(ok);

    QVariantList list = HistoryRepository::loadTtsHistory(dataDir);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first().toMap().value(QStringLiteral("text")).toString(), QStringLiteral("hello"));

    // Cleanup
    ok = HistoryRepository::clearTtsHistory(dataDir, errorMsg);
    QVERIFY(ok);
    list = HistoryRepository::loadTtsHistory(dataDir);
    QCOMPARE(list.size(), 0);
}

void TestHistory::testHistoryService()
{
    qDebug() << "--- START: testHistoryService ---";
    TtsEngine tts;
    AudioRecorder recorder;

    HistoryService service(&tts, &recorder);

    // Test asynchronous initial load
    QSignalSpy spyTts(&service, &HistoryService::ttsHistoryChanged);
    QSignalSpy spyStt(&service, &HistoryService::sttHistoryChanged);

    // Wait for initial load to finish to avoid race condition
    if (service.ttsHistory().isEmpty() && spyTts.isEmpty()) {
        spyTts.wait(1000);
    }
    spyTts.clear();

    // Add item (async execution trigger)
    service.addTtsHistoryItem(QStringLiteral("async test"), QStringLiteral("model"), QStringLiteral("voice"));
    
    // Wait for the async task queue to complete
    QVERIFY(spyTts.wait(2000));
    QVERIFY(service.ttsHistory().size() > 0);
}

void TestHistory::testSttHistory()
{
    qDebug() << "--- START: testSttHistory ---";
    QString dataDir = m_tempDir.path();
    QVector<float> samples = {0.1f, -0.2f, 0.3f, -0.4f};
    QString errorMsg;
    bool ok = HistoryRepository::addSttHistoryItem(dataDir, QStringLiteral("test transcription"), QStringLiteral("model"), samples, errorMsg);
    QVERIFY(ok);

    QVariantList list = HistoryRepository::loadSttHistory(dataDir);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first().toMap().value(QStringLiteral("text")).toString(), QStringLiteral("test transcription"));

    ok = HistoryRepository::clearSttHistory(dataDir, errorMsg);
    QVERIFY(ok);
}

} // namespace LAStudio
