#pragma once
#include <QObject>
#include <QList>
#include <QHash>
#include "IModelSession.h"

namespace LAStudio {

enum class ResourceReleaseResult {
    NotInUse,
    Released,
    Pending,
    BusyProcessing,
    Failed
};

class SttEngine;
class TtsEngine;
class AlignmentExecutionService;
class VoiceIsolatorController;
class TranslationModelSession;
class TranslationEngine;
class LlmChatEngine;
class LlmChatModelSession;

class ModelSessionRegistry : public QObject {
    Q_OBJECT
public:
    explicit ModelSessionRegistry(SttEngine *sttEngine,
                                  TtsEngine *ttsEngine,
                                  TranslationEngine *translationEngine,
                                  LlmChatEngine *llmEngine,
                                  AlignmentExecutionService *alignment,
                                  VoiceIsolatorController *voiceIsolator,
                                  QObject *parent = nullptr);
    ModelSessionRegistry(SttEngine *sttEngine,
                         TtsEngine *ttsEngine,
                         AlignmentExecutionService *alignment,
                         VoiceIsolatorController *voiceIsolator,
                         QObject *parent = nullptr);
    ~ModelSessionRegistry() override = default;

    IModelSession *sessionForCapability(const QString &capabilityId) const;
    QList<IModelSession *> sessions() const;

    ResourceReleaseResult prepareRuntimeRemoval(const QString &runtimeId,
                                                 const QString &runtimeVersion);

    ResourceReleaseResult prepareModelRemoval(const QString &modelPath);

private:
    IModelSession *m_sttSession = nullptr;
    IModelSession *m_ttsSession = nullptr;
    IModelSession *m_alignmentSession = nullptr;
    IModelSession *m_voiceIsolatorSession = nullptr;
    IModelSession *m_translationSession = nullptr;
    IModelSession *m_llmSession = nullptr;
};

} // namespace LAStudio
