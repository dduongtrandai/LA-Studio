#include "TtsTextPreprocessor.h"

#include "vietnorm/Normalizer.h"

namespace LAStudio {
namespace {

const vietnorm::Normalizer *normalizer()
{
    static const std::unique_ptr<vietnorm::Normalizer> instance = vietnorm::Normalizer::create();
    return instance.get();
}

} // namespace

QString TtsTextPreprocessor::prepare(const QString &text,
                                     const QVariantMap &studioConfig,
                                     const QVariantMap &settings,
                                     QString *profileId)
{
    const TextNormalizationPolicy policy = TextNormalizationPolicy::fromStudioConfig(studioConfig);
    if (profileId) *profileId = policy.usesVietNorm() ? policy.profile : QStringLiteral("none");
    const bool skipNormalization = settings.value(QStringLiteral("skip_text_normalization"), false).toBool()
        || settings.value(QStringLiteral("skip_normalize"), false).toBool();
    if (!policy.usesVietNorm() || skipNormalization)
        return text;

    vietnorm::NormalizationOptions options;
    options.profile = policy.profile.compare(QStringLiteral("compatibility-0.2.3"), Qt::CaseInsensitive) == 0
        ? vietnorm::Profile::Compatibility023 : vietnorm::Profile::SafeVietnameseTtsV1;
    options.enableTransliteration = policy.transliteration;
    return normalizer()->normalize(QStringView(text), options).text;
}

} // namespace LAStudio
