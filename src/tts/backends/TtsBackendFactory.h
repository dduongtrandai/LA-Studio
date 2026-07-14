#pragma once
#include <memory>
#include <QVariantMap>
#include "TtsBackend.h"

namespace LAStudio {

class TtsBackendFactory {
public:
    static std::unique_ptr<TtsBackend> create(const QVariantMap &config);
};

} // namespace LAStudio
