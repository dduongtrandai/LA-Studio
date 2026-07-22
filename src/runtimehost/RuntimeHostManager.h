#pragma once

#include <QMutex>
#include <QWaitCondition>
#include <QHash>
#include <QString>

namespace LAStudio {

// Process-level admission control for GPU-backed runtime hosts. A host keeps
// its CUDA context alive while a model is loaded, so limiting resident hosts
// avoids VRAM thrashing without serializing CPU-only runtimes.
class RuntimeHostManager final {
public:
    static RuntimeHostManager &instance();

    bool acquire(const QString &runtimeFamily, bool gpu, QString *error = nullptr,
                 int timeoutMs = 30000);
    void release(const QString &runtimeFamily, bool gpu);

    int activeGpuHosts() const;
    static constexpr int maxGpuHosts() { return 2; }

private:
    RuntimeHostManager() = default;
    mutable QMutex m_mutex;
    QWaitCondition m_gpuAvailable;
    QHash<QString, int> m_activeByFamily;
    int m_activeGpuHosts = 0;
};

} // namespace LAStudio
