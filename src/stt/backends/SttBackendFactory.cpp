#include "SttBackendFactory.h"
#include "WhisperSttBackend.h"
#include "QwenSttBackend.h"
#include "NemotronSttBackend.h"
#include "runtimehost/HostedWhisperBackend.h"

#include <QByteArray>

namespace {
bool useHostedWhisper()
{
    const QByteArray value = qgetenv("LASTUDIO_RUNTIME_HOST").trimmed().toLower();
    return value != "0" && value != "off" && value != "false";
}

std::unique_ptr<LAStudio::SttBackend> createWhisperBackend()
{
    if (useHostedWhisper()) return std::make_unique<LAStudio::HostedWhisperBackend>();
    return std::make_unique<LAStudio::WhisperSttBackend>();
}
}

namespace LAStudio {

std::unique_ptr<SttBackend> SttBackendFactory::create(const QVariantMap &config)
{
    const QString backend = config.value("backend").toString().toLower();
    const QString runtimePath = config.value("runtimePath").toString();
    const QString modelPath = config.value("model").toString();

    if (!backend.isEmpty()) {
        if (backend.contains(QStringLiteral("whisper"))) {
            return createWhisperBackend();
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
        return createWhisperBackend();
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
    return createWhisperBackend();
}

} // namespace LAStudio
