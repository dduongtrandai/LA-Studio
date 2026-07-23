#pragma once

#include <QString>
#include <QVariantMap>

namespace LAStudio {

struct TextNormalizationPolicy final {
    QString policy = QStringLiteral("none");
    QString profile = QStringLiteral("safe-vi-tts-v1");
    bool transliteration = false;

    static TextNormalizationPolicy fromStudioConfig(const QVariantMap &studioConfig);
    bool usesVietNorm() const { return policy.compare(QStringLiteral("vietnorm"), Qt::CaseInsensitive) == 0; }
};

} // namespace LAStudio
