#pragma once
#include <QObject>

namespace LAStudio {

class TestModelsAndRuntimes : public QObject {
    Q_OBJECT

private slots:
    void cleanupTestCase();
    void testModelManagerConcreteModelDir();
    void testModelManagerResolvesSplitVirtualModelFiles();
    void testCapabilityFamilyModelSuitability();
    void testVoiceDesignFamiliesExposeRuntimeOptions();
    void testForcedAlignmentCatalogEntry();
    void testVoiceIsolationRuntimeCatalogEntry();
    void testProcessRuntimeManifest();
    void testProcessRuntimeRejectsMissingEntrypoint();
    void testOptionalLlamaTranslationRuntimeLoad();
    void testLlamaCatalogIncludesAllWindowsX64Runtimes();
    void testVieNeuV3CatalogIncludesMossExternalData();
    void testCapabilityFamilyModelAcceptsExistingModelFiles();
    void testQwen3TtsUsesAutomaticFrameLimit();
    void testQwen3TtsDoesNotExposeUnsupportedLengthScale();
    void testLogViewServicePending();
    void testStudioConfigurationResolver();
};

} // namespace LAStudio
