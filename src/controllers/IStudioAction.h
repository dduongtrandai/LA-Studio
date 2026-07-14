#pragma once
#include <QObject>
#include <QString>
#include "core/StudioSelectionRepository.h"

namespace LAStudio {

enum class StudioState {
    Unloaded,
    Loading,
    Unloading,
    Ready,
    Processing,
    Error
};

class IStudioAction : public QObject {
    Q_OBJECT
public:
    explicit IStudioAction(QObject *parent = nullptr) : QObject(parent) {}
    ~IStudioAction() override = default;

    virtual QString capabilityId() const = 0;
    virtual QString activeConfigurationSignature() const = 0;
    virtual StudioState state() const = 0;
    virtual QString errorDetail() const = 0;

    virtual void load(const StudioConfiguration &configuration) = 0;
    virtual void unload() = 0;
    virtual void reload() = 0;

signals:
    void stateChanged();
    void activeConfigurationChanged();
    void errorOccurred(const QString &message);
};

} // namespace LAStudio
