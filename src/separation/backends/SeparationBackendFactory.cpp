#include "SeparationBackendFactory.h"
#include "sherpa/SherpaOnnxSeparationBackend.h"

namespace LAStudio {

SeparationBackendFactory::SeparationBackendFactory()
{
    registerBuiltins();
}

bool SeparationBackendFactory::registerBackend(const QString &backendId, Creator creator)
{
    if (backendId.isEmpty()) return false;
    if (m_creators.contains(backendId)) return false; // Reject duplicate
    m_creators.insert(backendId, creator);
    return true;
}

std::unique_ptr<SeparationBackend> SeparationBackendFactory::createBackend(const QString &backendId) const
{
    if (!m_creators.contains(backendId)) return nullptr;
    return m_creators.value(backendId)();
}

bool SeparationBackendFactory::hasBackend(const QString &backendId) const
{
    return m_creators.contains(backendId);
}

void SeparationBackendFactory::registerBuiltins()
{
    registerBackend(QStringLiteral("sherpa-onnx"), []() {
        return std::make_unique<SherpaOnnxSeparationBackend>();
    });
}

} // namespace LAStudio
