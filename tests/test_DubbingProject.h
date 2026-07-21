#pragma once

#include <QObject>

namespace LAStudio {

class TestDubbingProject : public QObject
{
    Q_OBJECT
private slots:
    void roundTripsVersionedJson();
    void rejectsUnknownSchema();
    void mergesSegmentPatchesByStableId();
    void rejectsUnknownAndDuplicateSegmentPatches();
    void importingMediaDoesNotStartProcessing();
    void sourceSeparationExposesModelSelection();
    void rejectsRerunningUnsupportedStep();
    void transcriptionRequiresReadyModel();
    void alignmentRefinementFallsBackWithoutDependencies();
    void audioGenerationWaitsForCompletedSynthesis();
    void audioGenerationUsesSelectedVoiceForEverySegment();
    void audioMixRunsAsynchronously();
    void commitsMediaExportAtomically();
    void sourceTextEditInvalidatesWordTiming();
    void unchangedTextEditPreservesTranslationMetadata();
    void targetTextEditRefreshesDurationMetadata();
    void exportsSubtitlesAndReviewPackage();
    void segmentNormalizerSplitsLongAsrTranscript();
    void segmentNormalizerUsesAlignedWordBoundaries();
    void countsVietnameseSyllablesAndPlansBudget();
    void selectsImprovingDurationCandidate();
    void prefersWithinBudgetDurationCandidate();
    void prefersClosestRepairCandidateOutsideBudget();
    void buildsPauseAlignedTtsChunks();
    void extractsAlignedPauses();
    void roundTripsDurationSettings();
    void normalizesLmStudioTranslationFixConfiguration();
    void parsesLmStudioTranslationFixResponses();
    void fixesOnlyTranslationsAbovePhonemeBudget();
    void ranksPartialTranslationFixesByBudgetDistance();
};

} // namespace LAStudio
