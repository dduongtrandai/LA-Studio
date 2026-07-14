#include "SttBackendFactory.h"
#include "WhisperSttBackend.h"
#include "QwenSttBackend.h"
#include "NemotronSttBackend.h"

namespace LAStudio {

std::unique_ptr<SttBackend> SttBackendFactory::create(const QVariantMap &config)
{
    const QString backend = config.value("backend").toString().toLower();
    const QString runtimePath = config.value("runtimePath").toString();
    const QString modelPath = config.value("model").toString();

    if (!backend.isEmpty()) {
        if (backend.contains(QStringLiteral("whisper"))) {
            return std::make_unique<WhisperSttBackend>();
        }
        if (backend.contains(QStringLiteral("qwen3")) || backend.contains(QStringLiteral("crispasr"))) {
            return std::make_unique<QwenSttBackend>();
        }
        if (backend.contains(QStringLiteral("nemotron"))) {
            return std::make_unique<NemotronSttBackend>();
        }
        return nullptr;
    }

    const bool isWhisper = runtimePath.contains(QStringLiteral("whisper"), Qt::CaseInsensitive) ||
                           modelPath.contains(QStringLiteral("ggml"), Qt::CaseInsensitive) ||
                           modelPath.contains(QStringLiteral("whisper"), Qt::CaseInsensitive);

    if (isWhisper) {
        return std::make_unique<WhisperSttBackend>();
    }

    const bool isNemotron = modelPath.contains(QStringLiteral("nemotron"), Qt::CaseInsensitive);
    if (isNemotron) {
        return std::make_unique<NemotronSttBackend>();
    }

    const bool isQwen = runtimePath.contains(QStringLiteral("crispasr"), Qt::CaseInsensitive) ||
                         modelPath.contains(QStringLiteral("qwen3"), Qt::CaseInsensitive);

    if (isQwen) {
        return std::make_unique<QwenSttBackend>();
    }

    // STT currently supports Whisper family by default.
    return std::make_unique<WhisperSttBackend>();
}

} // namespace LAStudio
