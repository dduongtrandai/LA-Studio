#include "StudioCapabilityRegistry.h"
#include "controllers/app/StudioActions.h"
#include "controllers/app/AppController.h"
#include "controllers/models/ModelSessionRegistry.h"

namespace LAStudio {

StudioCapabilityRegistry* StudioCapabilityRegistry::instance()
{
    static StudioCapabilityRegistry s_instance;
    return &s_instance;
}

StudioCapabilityRegistry::StudioCapabilityRegistry(QObject *parent)
    : QObject(parent)
{
    // Register the built-in studio capabilities.
    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("stt"),
        QStringLiteral("Speech to Text"),
        QStringLiteral("studio-stt"),
        QStringLiteral("stt"),
        QStringLiteral("stt"),
        QStringLiteral("Speech-to-Text Studio"),
        QStringLiteral("STT Model Gallery")
    });

    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("tts"),
        QStringLiteral("Text to Speech"),
        QStringLiteral("studio-tts"),
        QStringLiteral("tts"),
        QStringLiteral("tts-shared"),
        QStringLiteral("TTS Studio"),
        QStringLiteral("TTS Model Gallery")
    });

    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("voice-cloning"),
        QStringLiteral("Voice Cloning"),
        QStringLiteral("studio-voice-cloning"),
        QStringLiteral("voice-cloning"),
        QStringLiteral("tts-shared"),
        QStringLiteral("Voice Cloning Studio"),
        QStringLiteral("Voice Cloning Model Gallery")
    });

    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("voice-design"),
        QStringLiteral("Voice Design"),
        QStringLiteral("studio-voice-design"),
        QStringLiteral("voice-design"),
        QStringLiteral("tts-shared"),
        QStringLiteral("Voice Design Studio"),
        QStringLiteral("Voice Design Model Gallery")
    });

    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("forced-alignment"),
        QStringLiteral("Alignment"),
        QStringLiteral("studio-alignment"),
        QStringLiteral("forced-alignment"),
        QStringLiteral("alignment"),
        QStringLiteral("Alignment Studio"),
        QStringLiteral("Alignment Model Setup")
    });

    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("voice-isolation"),
        QStringLiteral("Voice Isolator"),
        QStringLiteral("studio-voice-isolator"),
        QStringLiteral("voice-isolation"),
        QStringLiteral("voice-isolation"),
        QStringLiteral("Voice Isolator Studio"),
        QStringLiteral("Voice Isolation Model Setup")
    });
    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("translation"),
        QStringLiteral("Translation"),
        QStringLiteral("studio-translation"),
        QStringLiteral("translation"),
        QStringLiteral("translation"),
        QStringLiteral("Translation Studio"),
        QStringLiteral("Translation Model Gallery")
    });
    registerCapability(StudioCapabilityDescriptor{
        QStringLiteral("llm-chat"),
        QStringLiteral("LLM Chat"),
        QStringLiteral("studio-llm"),
        QStringLiteral("chat"),
        QStringLiteral("llm"),
        QStringLiteral("LLM Chat Studio"),
        QStringLiteral("LLM Model Gallery")
    });
}

void StudioCapabilityRegistry::registerCapability(const StudioCapabilityDescriptor &descriptor)
{
    m_descriptors.insert(descriptor.id, descriptor);
}

StudioCapabilityDescriptor StudioCapabilityRegistry::getCapability(const QString &capabilityId) const
{
    return m_descriptors.value(capabilityId);
}

QList<StudioCapabilityDescriptor> StudioCapabilityRegistry::getAllCapabilities() const
{
    return m_descriptors.values();
}

bool StudioCapabilityRegistry::hasCapability(const QString &capabilityId) const
{
    return m_descriptors.contains(capabilityId);
}

IStudioAction* StudioCapabilityRegistry::createAction(const QString &capabilityId, QObject *parent) const
{
    AppController *app = AppController::instance();
    if (!app) return nullptr;

    IModelSession *session = app->sessionRegistry() ? app->sessionRegistry()->sessionForCapability(capabilityId) : nullptr;
    if (!session) return nullptr;

    return new CapabilityStudioAction(capabilityId, session, parent);
}

bool StudioCapabilityRegistry::familySupportsCapability(const QVariantMap &family, const QString &capabilityId) const
{
    const QVariantList capabilities = family.value(QStringLiteral("capabilities")).toList();
    if (capabilities.contains(capabilityId)) {
        return true;
    }
    if (capabilityId == QStringLiteral("stt")) {
        return family.value(QStringLiteral("supportsStt")).toBool();
    }
    if (capabilityId == QStringLiteral("tts")) {
        return family.value(QStringLiteral("supportsTts")).toBool();
    }
    if (capabilityId == QStringLiteral("voice-cloning")) {
        return family.value(QStringLiteral("supportsCloning")).toBool();
    }
    if (capabilityId == QStringLiteral("translation")) return family.value(QStringLiteral("supportsTranslation")).toBool();
    if (capabilityId == QStringLiteral("llm-chat")) return family.value(QStringLiteral("capabilities")).toList().contains(QStringLiteral("llm-chat"));
    return false;
}

QString StudioCapabilityRegistry::familyDomain(const QString &capabilityId) const
{
    if (capabilityId == QStringLiteral("stt")) {
        return QStringLiteral("stt");
    }
    if (capabilityId == QStringLiteral("forced-alignment")) {
        // Alignment models share the speech model catalog, not the STT engine session.
        return QStringLiteral("stt");
    }
    if (capabilityId == QStringLiteral("voice-isolation")) {
        // Source-separation families are catalogued alongside speech models so
        // they can use the existing model gallery/modal without pretending to
        // be a TTS family.
        return QStringLiteral("stt");
    }
    if (capabilityId == QStringLiteral("translation")) return QStringLiteral("stt");
    if (capabilityId == QStringLiteral("llm-chat")) return QStringLiteral("llm");
    // TTS-shared studios resolve from the TTS family pool.
    return QStringLiteral("tts");
}

} // namespace LAStudio
