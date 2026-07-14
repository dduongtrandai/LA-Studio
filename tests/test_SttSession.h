#pragma once
#include <QObject>

namespace LAStudio {

class TestSttSession : public QObject {
    Q_OBJECT

private slots:
    void cleanupTestCase();
    void testSttAudioDecoder();
    void testSttSessionPendingLoads();
    void testSttSessionHistoryRoundTrip();
    void testSttSessionUrlPreview();
    void testSttSessionQmlNotifications();
    void testSttRecordingSourceSelection();
};

} // namespace LAStudio
