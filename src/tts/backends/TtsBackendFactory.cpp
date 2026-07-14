#include "TtsBackendFactory.h"
#include "KokoroBackend.h"
#include "KokoroVietnameseBackend.h"
#include "Qwen3Backend.h"
#include "VoxCpm2Backend.h"
#include "VibevoiceBackend.h"
#include "VieneuTtsBackend.h"
#include "OmnivoiceBackend.h"

namespace LAStudio {

std::unique_ptr<TtsBackend> TtsBackendFactory::create(const QVariantMap &config)
{
    const QString modelPath = config.value("model").toString();
    const QString runtimePath = config.value("runtimePath").toString();
    const QString backend = config.value("backend").toString().toLower();
    const QString pipelineProfile = config.value("pipelineProfile").toString();
    const QString familyId = config.value("familyId").toString();

    if (!backend.isEmpty()) {
        if (backend.contains(QStringLiteral("vieneu")))
            return std::make_unique<VieneuTtsBackend>();
        if (backend.contains(QStringLiteral("qwen3-tts")))
            return std::make_unique<Qwen3Backend>();
        if (backend.contains(QStringLiteral("voxcpm2")))
            return std::make_unique<VoxCpm2Backend>();
        if (backend.contains(QStringLiteral("kokoro-vietnamese")) ||
            backend.contains(QStringLiteral("kokoro_vi")))
            return std::make_unique<KokoroVietnameseBackend>();
        if (backend.contains(QStringLiteral("kokoro")))
            return std::make_unique<KokoroBackend>();
        if (backend.contains(QStringLiteral("vibevoice")))
            return std::make_unique<VibevoiceBackend>();
        if (backend.contains(QStringLiteral("omnivoice")))
            return std::make_unique<OmnivoiceBackend>();
        return nullptr;
    }

    bool isVieneu = runtimePath.contains("vieneu", Qt::CaseInsensitive) ||
                    pipelineProfile.contains("vieneu", Qt::CaseInsensitive) ||
                    familyId.contains("vieneu", Qt::CaseInsensitive);

    bool isQwen3 = runtimePath.contains("qwen3", Qt::CaseInsensitive) ||
                   modelPath.contains("qwen3-tts", Qt::CaseInsensitive) ||
                   familyId.contains("qwen3", Qt::CaseInsensitive);

    bool isVoxCpm2 = modelPath.contains("voxcpm2", Qt::CaseInsensitive) ||
                     familyId.contains("voxcpm2", Qt::CaseInsensitive);

    bool isKokoroVietnamese = runtimePath.contains("kokoro-vietnamese", Qt::CaseInsensitive) ||
                              runtimePath.contains("kokoro_vi", Qt::CaseInsensitive) ||
                              modelPath.contains("kokoro_vi", Qt::CaseInsensitive) ||
                              familyId.contains("kokoro-vietnamese", Qt::CaseInsensitive);

    bool isKokoro = (runtimePath.contains("crispasr", Qt::CaseInsensitive) ||
                     runtimePath.contains("kokoro", Qt::CaseInsensitive)) &&
                    !isQwen3 &&
                    !isVoxCpm2 &&
                    !isKokoroVietnamese;

    bool isVibe = runtimePath.contains("vibevoice", Qt::CaseInsensitive);
    bool isOmni = runtimePath.contains("omnivoice", Qt::CaseInsensitive) ||
                  modelPath.contains("omnivoice", Qt::CaseInsensitive) ||
                  familyId.contains("omnivoice", Qt::CaseInsensitive);

    if (isVieneu) {
        return std::make_unique<VieneuTtsBackend>();
    }
    if (isQwen3) {
        return std::make_unique<Qwen3Backend>();
    }
    if (isVoxCpm2) {
        return std::make_unique<VoxCpm2Backend>();
    }
    if (isKokoroVietnamese) {
        return std::make_unique<KokoroVietnameseBackend>();
    }
    if (isKokoro) {
        return std::make_unique<KokoroBackend>();
    }
    if (isVibe) {
        return std::make_unique<VibevoiceBackend>();
    }
    if (isOmni) {
        return std::make_unique<OmnivoiceBackend>();
    }
    return nullptr;
}

} // namespace LAStudio
