#pragma once

#include <QObject>

namespace LAStudio {

class TestRuntimeHostProtocol final : public QObject {
    Q_OBJECT

private slots:
    void framesSurviveFragmentationAndCoalescing();
    void rejectsProtocolMismatchAndOversizedPayload();
    void roundTripsCborMap();
    void roundTripsSharedAudioBuffer();
    void startsAndPingsHostProcess();
    void limitsGpuHostAdmission();
};

} // namespace LAStudio
