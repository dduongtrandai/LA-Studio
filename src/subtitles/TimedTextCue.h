#pragma once

#include <QVariantMap>
#include <QString>

namespace LAStudio {

// Format-neutral timed text. Parsers populate this type; speech and audio
// pipelines consume it without knowing whether the source was SRT or VTT.
struct TimedTextCue {
    QString id;
    int cueNumber = 0;
    QString text;
    qint64 startMs = 0;
    qint64 endMs = 0;

    QVariantMap toVariantMap() const
    {
        return {{QStringLiteral("id"), id},
                {QStringLiteral("cueNumber"), cueNumber},
                {QStringLiteral("text"), text},
                {QStringLiteral("startMs"), startMs},
                {QStringLiteral("endMs"), endMs},
                {QStringLiteral("state"), QStringLiteral("pending")}};
    }
};

} // namespace LAStudio
