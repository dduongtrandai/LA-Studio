#pragma once

#include <memory>
#include <QVariantMap>

namespace LAStudio {

class SttBackend;

class SttBackendFactory {
public:
    static std::unique_ptr<SttBackend> create(const QVariantMap &config);
};

} // namespace LAStudio
