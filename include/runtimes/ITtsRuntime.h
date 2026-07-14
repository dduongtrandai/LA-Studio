#pragma once

#include <QString>

namespace LAStudio {

class ITtsRuntime {
public:
    virtual ~ITtsRuntime() = default;
    virtual bool load(const QString &libPath) = 0;
    virtual bool isLoaded() const = 0;
    virtual QString errorString() const = 0;
};

} // namespace LAStudio
