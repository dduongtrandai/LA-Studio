#include "test_RuntimeHostProtocol.h"

#include "runtimehost/RuntimeHostProtocol.h"
#include "runtimehost/RuntimeHostSharedBuffer.h"
#include "runtimehost/RuntimeHostClient.h"
#include "runtimehost/RuntimeHostManager.h"

#include <QCborArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QtTest>

namespace LAStudio {

void TestRuntimeHostProtocol::framesSurviveFragmentationAndCoalescing()
{
    RuntimeHostFrameParser parser;
    const QByteArray first = encodeRuntimeHostFrame(RuntimeHostMessage::Ping, 7,
                                                    encodeRuntimeHostCbor(QCborMap{{QStringLiteral("x"), 1}}));
    const QByteArray second = encodeRuntimeHostFrame(RuntimeHostMessage::Cancel, 8);

    parser.append((first + second).left(3));
    QVERIFY(!parser.takeNext().has_value());
    parser.append((first + second).sliced(3));

    const auto decodedFirst = parser.takeNext();
    QVERIFY(decodedFirst.has_value());
    QCOMPARE(decodedFirst->message, RuntimeHostMessage::Ping);
    QCOMPARE(decodedFirst->requestId, quint64(7));

    const auto decodedSecond = parser.takeNext();
    QVERIFY(decodedSecond.has_value());
    QCOMPARE(decodedSecond->message, RuntimeHostMessage::Cancel);
    QCOMPARE(decodedSecond->requestId, quint64(8));
    QVERIFY(!parser.takeNext().has_value());
}

void TestRuntimeHostProtocol::rejectsProtocolMismatchAndOversizedPayload()
{
    QByteArray invalid = encodeRuntimeHostFrame(RuntimeHostMessage::Ping, 1);
    invalid[4] = char(kRuntimeHostProtocolMajor + 1);
    RuntimeHostFrameParser parser;
    parser.append(invalid);
    QString error;
    QVERIFY(!parser.takeNext(&error).has_value());
    QVERIFY(error.contains(QStringLiteral("major"), Qt::CaseInsensitive));

    QByteArray oversized = encodeRuntimeHostFrame(RuntimeHostMessage::Ping, 2);
    constexpr int payloadSizeOffset = 22;
    for (int i = 0; i < 7; ++i) oversized[payloadSizeOffset + i] = static_cast<char>(-1);
    oversized[payloadSizeOffset + 7] = static_cast<char>(0x7F);
    parser.clear();
    parser.append(oversized);
    error.clear();
    QVERIFY(!parser.takeNext(&error).has_value());
    QVERIFY(error.contains(QStringLiteral("large"), Qt::CaseInsensitive));
}

void TestRuntimeHostProtocol::roundTripsCborMap()
{
    const QCborMap input{{QStringLiteral("ok"), true},
                         {QStringLiteral("name"), QStringLiteral("omnivoice")},
                         {QStringLiteral("items"), QCborArray{1, 2, 3}}};
    QCborMap output;
    QString error;
    QVERIFY(decodeRuntimeHostCbor(encodeRuntimeHostCbor(input), &output, &error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(output.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(output.value(QStringLiteral("name")).toString(), QStringLiteral("omnivoice"));
    QCOMPARE(output.value(QStringLiteral("items")).toArray().size(), 3);
}

void TestRuntimeHostProtocol::roundTripsSharedAudioBuffer()
{
    const QVector<float> input{0.0f, -0.25f, 0.5f, 1.0f};
    RuntimeHostSharedBuffer owner;
    QCborMap descriptor;
    QString error;
    QVERIFY2(owner.createFromSamples(input, 16000, 1, &descriptor, &error), qPrintable(error));

    RuntimeHostSharedBuffer peer;
    QVERIFY2(peer.attach(descriptor, &error), qPrintable(error));
    QVector<float> output;
    QVERIFY2(peer.copyTo(&output, &error), qPrintable(error));
    QCOMPARE(output, input);
    QCOMPARE(descriptor.value(QStringLiteral("sampleRate")).toInteger(), qint64(16000));
    peer.detach();
    owner.detach();
}

void TestRuntimeHostProtocol::startsAndPingsHostProcess()
{
    const QString hostPath = QDir(QCoreApplication::applicationDirPath())
                                 .absoluteFilePath(QStringLiteral("LAStudioRuntimeHost.exe"));
    QVERIFY2(QFileInfo(hostPath).isFile(), qPrintable(hostPath));
    RuntimeHostClient client;
    QString error;
    QVERIFY2(client.start(hostPath, &error), qPrintable(error));
    QVERIFY2(client.ping(&error), qPrintable(error));
    QVERIFY2(client.shutdown(&error), qPrintable(error));

    // Reloading a model after a normal host shutdown must be able to reuse the
    // same client object and establish a fresh named-pipe endpoint.
    error.clear();
    QVERIFY2(client.start(hostPath, &error), qPrintable(error));
    QVERIFY2(client.ping(&error), qPrintable(error));
    QVERIFY2(client.shutdown(&error), qPrintable(error));
}

void TestRuntimeHostProtocol::limitsGpuHostAdmission()
{
    RuntimeHostManager &manager = RuntimeHostManager::instance();
    QString error;
    QVERIFY(manager.acquire(QStringLiteral("test-gpu-a"), true, &error, 0));
    QVERIFY(manager.acquire(QStringLiteral("test-gpu-b"), true, &error, 0));
    QVERIFY(!manager.acquire(QStringLiteral("test-gpu-c"), true, &error, 0));
    QCOMPARE(manager.activeGpuHosts(), 2);
    manager.release(QStringLiteral("test-gpu-a"), true);
    manager.release(QStringLiteral("test-gpu-b"), true);
    QCOMPARE(manager.activeGpuHosts(), 0);
}

} // namespace LAStudio
