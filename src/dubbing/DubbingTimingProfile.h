#pragma once

#include <QString>
#include <QVariantMap>

namespace LAStudio {

struct DubbingTimingProfile
{
    QString id;
    QString voiceSignature;
    QString language = QStringLiteral("vi");
    double phonemesPerSecond = 10.0;
    int sampleCount = 0;
    double confidence = 0.0;
    QString normalizerVersion = QStringLiteral("phoneme-v1");

    QVariantMap toVariantMap() const;
    static DubbingTimingProfile fromVariantMap(const QVariantMap &map);
};

class DubbingTimingProfileStore final
{
public:
    static QString profileId(const QString &voiceSignature, const QString &language,
                             const QString &normalizerVersion = QStringLiteral("phoneme-v1"));
    static bool load(const QString &id, DubbingTimingProfile &profile);
    static bool save(const DubbingTimingProfile &profile, QString *error = nullptr);
};

} // namespace LAStudio
