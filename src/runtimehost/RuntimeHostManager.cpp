#include "RuntimeHostManager.h"

#include <QElapsedTimer>

namespace LAStudio {

RuntimeHostManager &RuntimeHostManager::instance()
{
    static RuntimeHostManager manager;
    return manager;
}

bool RuntimeHostManager::acquire(const QString &runtimeFamily, bool gpu, QString *error,
                                 int timeoutMs)
{
    if (!gpu) {
        QMutexLocker locker(&m_mutex);
        m_activeByFamily[runtimeFamily]++;
        return true;
    }

    QMutexLocker locker(&m_mutex);
    QElapsedTimer timer;
    timer.start();
    while (m_activeGpuHosts >= maxGpuHosts()) {
        const int remaining = qMax(0, timeoutMs - static_cast<int>(timer.elapsed()));
        if (remaining == 0 || !m_gpuAvailable.wait(&m_mutex, remaining)) {
            if (error) *error = QStringLiteral("GPU RuntimeHost limit (%1) reached; timed out waiting for a CUDA context slot.")
                                      .arg(maxGpuHosts());
            return false;
        }
    }
    ++m_activeGpuHosts;
    m_activeByFamily[runtimeFamily]++;
    return true;
}

void RuntimeHostManager::release(const QString &runtimeFamily, bool gpu)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_activeByFamily.find(runtimeFamily);
    if (it != m_activeByFamily.end()) {
        if (--it.value() <= 0) m_activeByFamily.erase(it);
    }
    if (gpu && m_activeGpuHosts > 0) {
        --m_activeGpuHosts;
        m_gpuAvailable.wakeOne();
    }
}

int RuntimeHostManager::activeGpuHosts() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeGpuHosts;
}

} // namespace LAStudio
