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
    void rejectsRerunningUnsupportedStep();
    void transcriptionRequiresReadyModel();
    void alignmentRefinementFallsBackWithoutDependencies();
    void sourceTextEditInvalidatesWordTiming();
    void segmentNormalizerSplitsLongAsrTranscript();
    void segmentNormalizerUsesAlignedWordBoundaries();
};

} // namespace LAStudio
