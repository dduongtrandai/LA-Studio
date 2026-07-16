#pragma once

#include <QObject>

namespace LAStudio {

class TestStudioCapabilities : public QObject {
    Q_OBJECT

private slots:
    void testForcedAlignmentDescriptor();
    void testForcedAlignmentFamilyMatching();
    void testVoiceIsolationSessionRegistered();
    void testTranslationDescriptorAndSession();
};

} // namespace LAStudio
