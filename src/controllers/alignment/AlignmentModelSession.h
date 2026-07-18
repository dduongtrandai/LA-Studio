#pragma once

#include "IModelSession.h"

#include <QHash>

namespace LAStudio {

class AlignmentExecutionService;

class AlignmentModelSession final : public IModelSession {
    Q_OBJECT
public:
    explicit AlignmentModelSession(AlignmentExecutionService *service, QObject *parent = nullptr);
    ~AlignmentModelSession() override = default;

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
    std::optional<SessionConfiguration> resolveConfig(const StudioConfiguration &config) const;
    void setError(const QString &message);
    void clearError();

    AlignmentExecutionService *m_service = nullptr;
    QHash<QString, SessionConfiguration> m_loadedConfigs;
    QString m_activeSignature;
    QString m_error;
};

} // namespace LAStudio
