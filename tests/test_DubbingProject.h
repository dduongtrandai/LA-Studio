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
    void sourceTextEditInvalidatesWordTiming();
    void exportsSubtitlesAndReviewPackage();
    void segmentNormalizerSplitsLongAsrTranscript();
    void segmentNormalizerUsesAlignedWordBoundaries();
    void countsVietnameseSyllablesAndPlansBudget();
    void selectsImprovingDurationCandidate();
    void buildsPauseAlignedTtsChunks();
    void extractsAlignedPauses();
    void roundTripsDurationSettings();
};

} // namespace LAStudio
