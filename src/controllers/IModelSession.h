#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>
#include "core/StudioSelectionRepository.h"

namespace LAStudio {

enum class ModelSessionState {
    Unconfigured,
    Unloaded,
    Loading,
    Ready,
    Processing,
    Unloading,
    Error
};

struct SessionConfiguration {
    QString capabilityId;
    StudioConfiguration selection;
    QString runtimePath;
    QVariantMap familyConfig;
    QStringList resolvedModelPaths;
    QVariantMap resolvedPathsByRole;
    QString signature;
};

class IModelSession : public QObject {
    Q_OBJECT

public:
    explicit IModelSession(QObject *parent = nullptr) : QObject(parent) {}
    ~IModelSession() override = default;

    virtual ModelSessionState state() const = 0;
    virtual bool modelActive() const = 0;
    virtual bool canProcess() const = 0;

    virtual std::optional<SessionConfiguration> activeConfiguration() const = 0;
    virtual std::optional<SessionConfiguration> pendingConfiguration() const = 0;
    virtual QList<SessionConfiguration> loadedConfigurations() const = 0;
    virtual QString activeSignature() const = 0;

    virtual void requestLoad(const QString &capabilityId,
                             const StudioConfiguration &configuration) = 0;
    virtual void requestUnload(const QString &capabilityId) = 0;
    virtual void requestUnloadConfiguration(const QString &signature) = 0;
    virtual void activateConfiguration(const QString &signature) = 0;
    virtual void requestReload(const QString &capabilityId) = 0;

    virtual bool usesRuntime(const QString &runtimeId,
                             const QString &runtimeVersion) const = 0;
    virtual bool usesModelPath(const QString &modelPath) const = 0;

signals:
    void stateChanged();
    void activeConfigurationChanged();
    void pendingConfigurationChanged();
    void activeSignatureChanged();
    void errorOccurred(const QString &message);
};

} // namespace LAStudio
