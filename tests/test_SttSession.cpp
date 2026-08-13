#include "test_SttSession.h"
#include <QtTest>
#include <QSignalSpy>
#include <QThreadPool>
#include <QUrl>

#include "controllers/app/AppController.h"
#include "controllers/stt/SttSessionController.h"
#include "controllers/stt/SttAudioDecoder.h"
#include "controllers/models/ModelLifecycleController.h"
#include "core/StudioCapabilityRegistry.h"
#include "stt/SttEngine.h"
#include "stt/SttSubtitleService.h"
#include "audio/LongFormAudioSource.h"
#include <QTemporaryDir>
#include <QFile>

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

void TestSttSession::testSttSubtitleExport()
{
    const QVariantList segments{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("a")},
                    {QStringLiteral("start"), 1.234},
                    {QStringLiteral("end"), 3.456},
                    {QStringLiteral("text"), QStringLiteral("Hello world")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("b")},
                    {QStringLiteral("startMs"), 4000},
                    {QStringLiteral("endMs"), 5000},
                    {QStringLiteral("text"), QStringLiteral("Second cue")}}
    };
    const QVariantList cues = SttSubtitleService::cuesFromSegments(segments);
    QCOMPARE(cues.size(), 2);
    const QString srt = SttSubtitleService::toSrt(cues);
    QVERIFY(srt.contains(QStringLiteral("00:00:01,234 --> 00:00:03,456")));
    QVERIFY(srt.contains(QStringLiteral("Hello world")));
    const QString vtt = SttSubtitleService::toVtt(cues);
    QVERIFY(vtt.startsWith(QStringLiteral("WEBVTT\n")));
    QVERIFY(vtt.contains(QStringLiteral("00:00:01.234 --> 00:00:03.456")));
}

void TestSttSession::testLongFormRangeCacheReader()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath(QStringLiteral("audio.f32le"));
    QVector<float> source(16000 * 2);
    for (int i = 0; i < source.size(); ++i) source[i] = float(i) / float(source.size());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(reinterpret_cast<const char *>(source.constData()), source.size() * int(sizeof(float))) > 0);
    file.close();

    qint64 durationMs = 0;
    QString error;
    QVERIFY(LongFormAudioSource::probeDurationMs(path, durationMs, &error));
    QCOMPARE(durationMs, qint64(2000));
    const QVector<float> range = LongFormAudioSource::readMono16kRange(path, 500, 250, &error);
    QCOMPARE(range.size(), 4000);
    QVERIFY(qAbs(range.first() - source.at(8000)) < 0.0001f);
}

} // namespace LAStudio
