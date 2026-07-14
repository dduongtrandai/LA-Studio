#include "test_SttSession.h"
#include <QtTest>
#include <QSignalSpy>
#include <QThreadPool>
#include <QUrl>

#include "controllers/AppController.h"
#include "controllers/SttSessionController.h"
#include "controllers/SttAudioDecoder.h"
#include "controllers/ModelLifecycleController.h"
#include "core/StudioCapabilityRegistry.h"
#include "stt/SttEngine.h"

namespace LAStudio {

void TestSttSession::cleanupTestCase()
{
    QThreadPool::globalInstance()->waitForDone();
}

void TestSttSession::testSttAudioDecoder()
{
    qDebug() << "--- START: testSttAudioDecoder ---";
    SttAudioDecoder decoder;
    QSignalSpy spyError(&decoder, &SttAudioDecoder::errorOccurred);

    decoder.startDecode(QStringLiteral("nonexistent.wav"));
    QVERIFY(spyError.size() > 0 || spyError.wait(1000));
}

void TestSttSession::testSttSessionPendingLoads()
{
    qDebug() << "--- START: testSttSessionPendingLoads ---";

    QList<QString> startedLoads;
    ModelLifecycleController lifecycle(
        [](const StudioConfiguration &config) {
            SessionConfiguration resolved;
            resolved.capabilityId = config.capabilityId;
            resolved.selection = config;
            resolved.signature = config.selectedFiles.value(QStringLiteral("model")).toString();
            return std::optional<SessionConfiguration>(resolved);
        },
        [&startedLoads](const SessionConfiguration &config) {
            startedLoads.append(config.signature);
        },
        []() {});

    StudioConfiguration first;
    first.capabilityId = QStringLiteral("stt");
    first.familyId = QStringLiteral("whisper.cpp");
    first.selectedFiles.insert(QStringLiteral("model"), QStringLiteral("ggml-tiny.bin"));
    StudioConfiguration second = first;
    second.selectedFiles.insert(QStringLiteral("model"), QStringLiteral("ggml-base.bin"));

    lifecycle.requestLoad(QStringLiteral("stt"), first);
    lifecycle.requestLoad(QStringLiteral("stt"), second);
    QCOMPARE(startedLoads, QList<QString>{QStringLiteral("ggml-tiny.bin")});

    lifecycle.onLoadSuccess();
    QVERIFY(lifecycle.activeConfiguration().has_value());
    QCOMPARE(lifecycle.activeSignature(), QStringLiteral("ggml-tiny.bin"));
    QCOMPARE(startedLoads, QList<QString>({QStringLiteral("ggml-tiny.bin"), QStringLiteral("ggml-base.bin")}));

    lifecycle.onLoadSuccess();
    QCOMPARE(lifecycle.activeSignature(), QStringLiteral("ggml-base.bin"));
}

void TestSttSession::testSttSessionHistoryRoundTrip()
{
    qDebug() << "--- START: testSttSessionHistoryRoundTrip ---";
    SttSessionController session;

    QString savedText = QStringLiteral("Saved history transcription text");
    QString savedPath = QStringLiteral("E:/saved_audio.wav");
    session.loadHistoryItem(savedText, savedPath);

    // Verify transcript is restored
    QCOMPARE(session.transcript(), savedText);

    // Verify file input path and normalized URL are set
    QCOMPARE(session.inputPath(), QStringLiteral("E:/saved_audio.wav"));
    QCOMPARE(session.inputUrl(), QUrl::fromLocalFile(QStringLiteral("E:/saved_audio.wav")));
}

void TestSttSession::testSttSessionUrlPreview()
{
    qDebug() << "--- START: testSttSessionUrlPreview ---";
    SttSessionController session;

    // Windows local path
    session.selectFileInput(QStringLiteral("C:/audio.wav"));
    QCOMPARE(session.inputUrl(), QUrl::fromLocalFile(QStringLiteral("C:/audio.wav")));

    // Standard file URL
    session.selectFileInput(QStringLiteral("file:///D:/folder/audio.wav"));
    QCOMPARE(session.inputUrl(), QUrl::fromLocalFile(QStringLiteral("D:/folder/audio.wav")));
}

void TestSttSession::testSttSessionQmlNotifications()
{
    qDebug() << "--- START: testSttSessionQmlNotifications ---";
    SttSessionController session;
    SttEngine* engine = AppController::instance()->stt();
    QVERIFY(engine != nullptr);

    QSignalSpy spyProcessing(&session, &SttSessionController::processingChanged);
    QSignalSpy spyTranscript(&session, &SttSessionController::transcriptChanged);

    // 1. Verify transcriptChanged when cleared
    session.clearTranscript();
    QCOMPARE(spyTranscript.size(), 1);

    // 2. Verify processingChanged when transcribing
    engine->transcribeSamples({0.1f});
    QTRY_COMPARE_WITH_TIMEOUT(spyProcessing.size(), 1, 500);
}

void TestSttSession::testSttRecordingSourceSelection()
{
    SttSessionController session;
    AudioRecorder *recorder = AppController::instance()->recorder();
    QVERIFY(recorder != nullptr);

    session.startRecording(true);
    QVERIFY(recorder->recordSystemAudio());

    session.startRecording(false);
    QVERIFY(!recorder->recordSystemAudio());
}

} // namespace LAStudio
