#pragma once

#include <QObject>

namespace LAStudio {

class TestDubbingProject : public QObject
{
    Q_OBJECT
private slots:
    void roundTripsVersionedJson();
    void migratesLegacyProjectsToLlmRewritePipeline();
    void rejectsUnknownSchema();
    void mergesSegmentPatchesByStableId();
    void rejectsUnknownAndDuplicateSegmentPatches();
    void importingMediaDoesNotStartProcessing();
    void automaticWorkflowLocksSettingsUntilPaused();
    void sourceSeparationExposesModelSelection();
    void targetLanguageUpdatesVoiceNodeLanguage();
    void rejectsRerunningUnsupportedStep();
    void transcriptionRequiresReadyModel();
    void alignmentRefinementFallsBackWithoutDependencies();
    void audioGenerationWaitsForCompletedSynthesis();
    void audioGenerationUsesSelectedVoiceForEverySegment();
    void selectsBestAutomaticVoiceReference();
    void audioGenerationUsesAutomaticVoiceReference();
    void audioMixRunsAsynchronously();
    void audioMixCreatesIndependentVocalStem();
    void commitsMediaExportAtomically();
    void sourceTextEditInvalidatesWordTiming();
    void unchangedTextEditPreservesTranslationMetadata();
    void targetTextEditRefreshesDurationMetadata();
    void exportsSubtitlesAndReviewPackage();
    void segmentNormalizerSplitsLongAsrTranscript();
    void segmentNormalizerUsesAlignedWordBoundaries();
    void segmentNormalizerRebuildsAcrossAsrBoundaries();
    void countsVietnameseSyllablesAndPlansBudget();
    void selectsImprovingDurationCandidate();
    void prefersWithinBudgetDurationCandidate();
    void prefersClosestRepairCandidateOutsideBudget();
    void buildsPauseAlignedTtsChunks();
    void extractsAlignedPauses();
    void roundTripsDurationSettings();
    void normalizesLmStudioTranslationFixConfiguration();
    void parsesLmStudioTranslationFixResponses();
    void fixesOnlyTranslationsOverPhonemeLimit();
    void ranksPartialTranslationFixesByBudgetDistance();
};

} // namespace LAStudio
