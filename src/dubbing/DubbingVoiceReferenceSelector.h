#pragma once

#include <QString>
#include <QVariantList>

namespace LAStudio {

struct DubbingVoiceReference
{
    QString audioPath;
    QString referenceText;
    QString speakerId;
    qint64 startMs = 0;
    qint64 endMs = 0;
    double qualityScore = 0.0;
    QString error;

    bool isValid() const { return !audioPath.isEmpty() && error.isEmpty(); }
};

// Selects a clean, speech-dense source window and writes it as a reusable WAV
// reference for zero-shot voice cloning. The score is a local signal-quality
// estimate; it deliberately does not claim to be a UTMOS prediction.
class DubbingVoiceReferenceSelector
{
public:
    static DubbingVoiceReference select(const QString &sourceAudioPath,
                                         const QVariantList &segments,
                                         const QString &projectPath,
                                         const QString &speakerId = QString());
};

} // namespace LAStudio
