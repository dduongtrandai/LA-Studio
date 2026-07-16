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

void TestStudioCapabilities::testTranslationDescriptorAndSession()
{
    StudioCapabilityRegistry *capabilities = StudioCapabilityRegistry::instance();
    const StudioCapabilityDescriptor descriptor = capabilities->getCapability(QStringLiteral("translation"));
    QCOMPARE(descriptor.displayName, QStringLiteral("Translation"));
    QCOMPARE(descriptor.routeId, QStringLiteral("studio-translation"));
    QCOMPARE(descriptor.sharedEngineGroup, QStringLiteral("translation"));
    QCOMPARE(capabilities->familyDomain(QStringLiteral("translation")), QStringLiteral("stt"));
    QVariantMap family{{QStringLiteral("capabilities"), QVariantList{QStringLiteral("translation")}}, {QStringLiteral("supportsTranslation"), true}};
    QVERIFY(capabilities->familySupportsCapability(family, QStringLiteral("translation")));
    QVERIFY(AppController::instance()->sessionRegistry()->sessionForCapability(QStringLiteral("translation")));

    bool m2m100Visible = false;
    for (const QVariant &entry : AppController::instance()->registry()->sttFamilies()) {
        const QVariantMap candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == QStringLiteral("m2m100-418m")) {
            m2m100Visible = candidate.value(QStringLiteral("capabilities")).toList().contains(QStringLiteral("translation"));
            break;
        }
    }
    QVERIFY2(m2m100Visible, "Translation families must be included in the speech catalog view used by Translation Studio.");
}

} // namespace LAStudio
