#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace LAStudio {

// Versioned, dependency-free representation of a dubbing edit session.
// Generated audio is deliberately kept out of the JSON; it belongs in the
// project's cache directory and can be regenerated from the segment fields.
class DubbingProject
{
public:
    static constexpr int CurrentSchemaVersion = 2;

    QString projectPath;
    QString sourceMediaPath;
    QString sourceHash;
    QString masterAudioPath;
    QString analysisAudioPath;
    QString backgroundAudioPath;
    qint64 sourceDurationMs = 0;
    int sourceSampleRate = 0;
    int sourceChannels = 0;
    bool sourceIsVideo = false;
    QString sourceLanguage = QStringLiteral("en");
    QString targetLanguage = QStringLiteral("vi");
    QVariantList speakers;
    QVariantList segments;

    bool save(QString *error = nullptr) const;
    static bool load(const QString &path, DubbingProject &project, QString *error = nullptr);

    static bool mergeSegmentPatches(const QVariantList &segments,
                                    const QVariantList &patches,
                                    QVariantList &merged,
                                    QString *error = nullptr);

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &json, DubbingProject &project, QString *error = nullptr);
};

} // namespace LAStudio
