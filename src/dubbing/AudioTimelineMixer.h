#pragma once

#include <QVector>
#include <QString>
#include <QVariantList>
#include <QAtomicInteger>

namespace LAStudio {

class AudioTimelineMixer
{
public:
    static QVector<float> resampleToCount(const QVector<float> &source, int targetCount);
    static QString vocalStemPath(const QString &outputPath);

    static bool mixSegments(const QVariantList &segments, const QString &outputPath, QString *error = nullptr);
    static bool mixSegments(const QVariantList &segments, const QString &outputPath,
                            const QString &backgroundPath, QString *error = nullptr,
                            QAtomicInteger<bool> *cancel = nullptr);
    static bool mixSegments(const QVariantList &segments, const QString &outputPath,
                            const QString &backgroundPath, const QString &vocalOutputPath,
                            QString *error = nullptr, QAtomicInteger<bool> *cancel = nullptr);
};

} // namespace LAStudio
