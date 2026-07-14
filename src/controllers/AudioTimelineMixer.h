#pragma once

#include <QVector>
#include <QString>
#include <QVariantList>

namespace LAStudio {

class AudioTimelineMixer
{
public:
    static QVector<float> resampleToCount(const QVector<float> &source, int targetCount);

    static bool mixSegments(const QVariantList &segments, const QString &outputPath, QString *error = nullptr);
    static bool mixSegments(const QVariantList &segments, const QString &outputPath,
                            const QString &backgroundPath, QString *error = nullptr);
};

} // namespace LAStudio
