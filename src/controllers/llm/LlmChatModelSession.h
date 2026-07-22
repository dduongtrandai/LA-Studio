#pragma once

#include "controllers/models/IModelSession.h"
#include <QHash>

namespace LAStudio {
class LlmChatEngine;

class LlmChatModelSession final : public IModelSession
{
    Q_OBJECT
public:
    explicit LlmChatModelSession(LlmChatEngine *engine, QObject *parent = nullptr);
    ModelSessionState state() const override;
    bool modelActive() const override;
    bool canProcess() const override;
    std::optional<SessionConfiguration> activeConfiguration() const override;
    std::optional<SessionConfiguration> pendingConfiguration() const override;
    QList<SessionConfiguration> loadedConfigurations() const override;
    QString activeSignature() const override;
    void requestLoad(const QString &capabilityId, const StudioConfiguration &configuration) override;
    void requestUnload(const QString &capabilityId) override;
    void requestUnloadConfiguration(const QString &signature) override;
    void activateConfiguration(const QString &signature) override;
    void requestReload(const QString &capabilityId) override;
    bool usesRuntime(const QString &runtimeId, const QString &runtimeVersion) const override;
    bool usesModelPath(const QString &modelPath) const override;
private:
    std::optional<SessionConfiguration> resolveConfig(const StudioConfiguration &configuration) const;
    LlmChatEngine *m_engine = nullptr;
    QHash<QString, SessionConfiguration> m_loaded;
    QString m_activeSignature;
    QString m_error;
};
} // namespace LAStudio
