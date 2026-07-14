#pragma once

#include "SeparationBackend.h"
#include <QString>
#include <QMap>
#include <functional>
#include <memory>

namespace LAStudio {

class SeparationBackendFactory {
public:
    using Creator = std::function<std::unique_ptr<SeparationBackend>()>;
    
    SeparationBackendFactory();
    ~SeparationBackendFactory() = default;
    
    bool registerBackend(const QString &backendId, Creator creator);
    std::unique_ptr<SeparationBackend> createBackend(const QString &backendId) const;
    bool hasBackend(const QString &backendId) const;
    
private:
    void registerBuiltins();
    QMap<QString, Creator> m_creators;
};

} // namespace LAStudio
