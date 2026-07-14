#pragma once

#include "IModelSession.h"

#include <QHash>

namespace LAStudio {

class VoiceIsolatorController;

class VoiceIsolatorModelSession final : public IModelSession {
    Q_OBJECT
public:
    explicit VoiceIsolatorModelSession(VoiceIsolatorController *controller,
                                       QObject *parent = nullptr);
    ~VoiceIsolatorModelSession() override = default;

    ModelSessionState state() const override;
    bool modelActive() const override;
    bool canProcess() const override;

    std::optional<SessionConfiguration> activeConfiguration() const override;
    std::optional<SessionConfiguration> pendingConfiguration() const override;
    QList<SessionConfiguration> loadedConfigurations() const override;
    QString activeSignature() const override;

    void requestLoad(const QString &capabilityId,
                     const StudioConfiguration &configuration) override;
    void requestUnload(const QString &capabilityId) override;
    void requestUnloadConfiguration(const QString &signature) override;
    void activateConfiguration(const QString &signature) override;
    void requestReload(const QString &capabilityId) override;

    bool usesRuntime(const QString &runtimeId,
                     const QString &runtimeVersion) const override;
    bool usesModelPath(const QString &modelPath) const override;

private:
    std::optional<SessionConfiguration> resolveConfig(const StudioConfiguration &config);
    void applyConfiguration(const SessionConfiguration &config);
    void setError(const QString &message);
    void clearError();

    VoiceIsolatorController *m_controller = nullptr;
    QHash<QString, SessionConfiguration> m_loadedConfigs;
    QString m_activeSignature;
    QString m_error;
};

} // namespace LAStudio
