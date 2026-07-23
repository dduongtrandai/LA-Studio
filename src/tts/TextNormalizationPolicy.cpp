#include "TextNormalizationPolicy.h"

namespace LAStudio {

TextNormalizationPolicy TextNormalizationPolicy::fromStudioConfig(const QVariantMap &studioConfig)
{
    TextNormalizationPolicy result;
    const QVariantMap config = studioConfig.value(QStringLiteral("textNormalization")).toMap();
    if (config.isEmpty()) return result;
    result.policy = config.value(QStringLiteral("policy"), result.policy).toString();
    result.profile = config.value(QStringLiteral("profile"), result.profile).toString();
    result.transliteration = config.value(QStringLiteral("transliteration"), false).toBool();
    return result;
}

} // namespace LAStudio
