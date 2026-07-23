#pragma once

namespace vietnorm {

enum class Profile {
    Compatibility023,
    SafeVietnameseTtsV1,
};

struct NormalizationOptions {
    Profile profile = Profile::SafeVietnameseTtsV1;
    bool enablePreprocessing = true;
    bool enableTransliteration = false;
};

} // namespace vietnorm
