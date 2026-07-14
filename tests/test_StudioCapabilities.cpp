#include "test_StudioCapabilities.h"

#include <QtTest>

#include "core/StudioCapabilityRegistry.h"
#include "controllers/AppController.h"
#include "controllers/ModelSessionRegistry.h"

namespace LAStudio {

void TestStudioCapabilities::testForcedAlignmentDescriptor()
{
    StudioCapabilityRegistry *registry = StudioCapabilityRegistry::instance();

    QVERIFY(registry->hasCapability(QStringLiteral("forced-alignment")));
    const StudioCapabilityDescriptor descriptor = registry->getCapability(QStringLiteral("forced-alignment"));
    QCOMPARE(descriptor.displayName, QStringLiteral("Alignment"));
    QCOMPARE(descriptor.routeId, QStringLiteral("studio-alignment"));
    QCOMPARE(descriptor.pageTitle, QStringLiteral("Alignment Studio"));
    QCOMPARE(descriptor.sharedEngineGroup, QStringLiteral("alignment"));
    QCOMPARE(registry->familyDomain(QStringLiteral("forced-alignment")), QStringLiteral("stt"));
}

void TestStudioCapabilities::testForcedAlignmentFamilyMatching()
{
    StudioCapabilityRegistry *registry = StudioCapabilityRegistry::instance();
    QVariantMap family;
    family.insert(QStringLiteral("capabilities"), QVariantList{QStringLiteral("forced-alignment")});

    QVERIFY(registry->familySupportsCapability(family, QStringLiteral("forced-alignment")));
    QVERIFY(!registry->familySupportsCapability(family, QStringLiteral("tts")));
}

void TestStudioCapabilities::testVoiceIsolationSessionRegistered()
{
    StudioCapabilityRegistry *capabilities = StudioCapabilityRegistry::instance();
    const StudioCapabilityDescriptor descriptor =
        capabilities->getCapability(QStringLiteral("voice-isolation"));

    QCOMPARE(descriptor.sharedEngineGroup, QStringLiteral("voice-isolation"));
    QCOMPARE(capabilities->familyDomain(QStringLiteral("voice-isolation")), QStringLiteral("stt"));

    AppController *app = AppController::instance();
    QVERIFY(app);
    QVERIFY(app->sessionRegistry());
    QVERIFY2(app->sessionRegistry()->sessionForCapability(QStringLiteral("voice-isolation")),
             "Voice Isolation must have a model session so Studio load actions have a target");
}

} // namespace LAStudio
