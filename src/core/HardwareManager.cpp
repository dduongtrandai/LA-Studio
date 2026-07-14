#include "HardwareManager.h"
#include "Logger.h"
#include <QDebug>
#include <QSysInfo>
#include <QStringList>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <intrin.h>
#include <dxgi1_6.h>
#include <vector>

#pragma comment(lib, "dxgi.lib")
#endif

namespace LAStudio {

static HardwareManager* s_instance = nullptr;

HardwareManager::HardwareManager(QObject *parent) : QObject(parent) {
    s_instance = this;
    detectHardware();
    
    m_usageTimer = new QTimer(this);
    connect(m_usageTimer, &QTimer::timeout, this, &HardwareManager::updateResourceUsage);
    m_usageTimer->start(2000); // Update every 2 seconds
}

HardwareManager* HardwareManager::instance() {
    if (!s_instance) {
        s_instance = new HardwareManager();
    }
    return s_instance;
}

HardwareManager* HardwareManager::create(QQmlEngine *, QJSEngine *) {
    return instance();
}

QVariantMap HardwareManager::runtimeCompatibility(const QVariantMap &runtime) const {
    const QString id = runtime.value(QStringLiteral("id")).toString().toLower();
    const QString name = runtime.value(QStringLiteral("name")).toString().toLower();
    const QString label = runtime.value(QStringLiteral("label")).toString().toLower();
    const QString asset = runtime.value(QStringLiteral("asset")).toString().toLower();
    const QString haystack = QStringList{id, name, label, asset}.join(QStringLiteral(" "));

    QVariantMap result;
    result.insert(QStringLiteral("compatible"), true);
    result.insert(QStringLiteral("kind"), QStringLiteral("cpu"));
    result.insert(QStringLiteral("title"), QStringLiteral("Compatible"));
    result.insert(QStringLiteral("detail"), QStringLiteral("Runs on the detected CPU."));

    const QString disabledReason = runtime.value(QStringLiteral("disabledReason")).toString().trimmed();
    if (!disabledReason.isEmpty()) {
        result.insert(QStringLiteral("compatible"), false);
        result.insert(QStringLiteral("title"), QStringLiteral("Unavailable"));
        result.insert(QStringLiteral("detail"), disabledReason);
        return result;
    }

    if (haystack.contains(QStringLiteral("cuda"))) {
        const bool hasNvidiaGpu = std::any_of(m_gpus.cbegin(), m_gpus.cend(), [](const QVariant &gpuValue) {
            const QVariantMap gpu = gpuValue.toMap();
            return gpu.value(QStringLiteral("name")).toString().contains(QStringLiteral("NVIDIA"), Qt::CaseInsensitive);
        });

        result.insert(QStringLiteral("kind"), QStringLiteral("cuda"));
        result.insert(QStringLiteral("compatible"), hasNvidiaGpu);
        result.insert(QStringLiteral("title"), hasNvidiaGpu ? QStringLiteral("Compatible") : QStringLiteral("Unavailable"));
        result.insert(QStringLiteral("detail"), hasNvidiaGpu
            ? QStringLiteral("NVIDIA GPU detected.")
            : QStringLiteral("Requires an NVIDIA CUDA-capable GPU."));
        return result;
    }

    if (haystack.contains(QStringLiteral("vulkan"))) {
        const bool hasHardwareGpu = !m_gpus.isEmpty();
        bool isCompatible = hasHardwareGpu;
        QString detail = QStringLiteral("Hardware GPU detected.");

        // Intel integrated GPUs have driver bugs/limitations that crash the Omnivoice Vulkan backend
        if (haystack.contains(QStringLiteral("omnivoice"))) {
            bool hasIntelGpu = std::any_of(m_gpus.cbegin(), m_gpus.cend(), [](const QVariant &gpuValue) {
                const QVariantMap gpu = gpuValue.toMap();
                return gpu.value(QStringLiteral("name")).toString().contains(QStringLiteral("Intel"), Qt::CaseInsensitive);
            });
            if (hasIntelGpu) {
                isCompatible = false;
                detail = QStringLiteral("OmniVoice Vulkan backend is unstable and disabled on Intel integrated GPUs.");
            }
        }

        result.insert(QStringLiteral("kind"), QStringLiteral("vulkan"));
        result.insert(QStringLiteral("compatible"), isCompatible);
        result.insert(QStringLiteral("title"), isCompatible ? QStringLiteral("Compatible") : QStringLiteral("Unavailable"));
        result.insert(QStringLiteral("detail"), isCompatible ? detail : (hasHardwareGpu ? detail : QStringLiteral("Requires a hardware GPU with Vulkan support.")));
        return result;
    }

    const bool supportedCpu = m_cpuArchitecture.compare(QStringLiteral("x86_64"), Qt::CaseInsensitive) == 0
        || m_cpuArchitecture.compare(QStringLiteral("arm64"), Qt::CaseInsensitive) == 0;
    result.insert(QStringLiteral("compatible"), supportedCpu);
    result.insert(QStringLiteral("title"), supportedCpu ? QStringLiteral("Compatible") : QStringLiteral("Unavailable"));
    result.insert(QStringLiteral("detail"), supportedCpu
        ? QStringLiteral("Uses system RAM and CPU instructions.")
        : QStringLiteral("Requires a supported 64-bit CPU."));
    return result;
}

void HardwareManager::detectHardware() {
#ifdef Q_OS_WIN
    // CPU Name
    int cpuInfo[4] = { -1 };
    char cpuBrandString[0x40];
    memset(cpuBrandString, 0, sizeof(cpuBrandString));
    __cpuid(cpuInfo, 0x80000002);
    memcpy(cpuBrandString, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000003);
    memcpy(cpuBrandString + 16, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000004);
    memcpy(cpuBrandString + 32, cpuInfo, sizeof(cpuInfo));
    m_cpuName = QString::fromLatin1(cpuBrandString).trimmed();

    // CPU Architecture
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
        m_cpuArchitecture = "x86_64";
    else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
        m_cpuArchitecture = "ARM64";
    else if (sysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
        m_cpuArchitecture = "x86";
    else
        m_cpuArchitecture = "Unknown";

    // CPU Flags (AVX, AVX2)
    QStringList flags;
    __cpuid(cpuInfo, 1);
    if (cpuInfo[2] & (1 << 28)) flags << "AVX";
    
    __cpuid(cpuInfo, 7);
    if (cpuInfo[1] & (1 << 5)) flags << "AVX2";
    if (cpuInfo[1] & (1 << 16)) flags << "AVX512";
    
    m_cpuFlags = flags.join(" ");

    // RAM Total
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        m_ramTotal = static_cast<double>(memStatus.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
    }

    // GPUs
    m_gpus.clear();
    m_vramTotal = 0;
    
    IDXGIFactory1* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory))) {
        IDXGIAdapter1* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);
            
            // Skip basic render drivers
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                pAdapter->Release();
                continue;
            }

            QVariantMap gpu;
            gpu["name"] = QString::fromWCharArray(desc.Description);
            gpu["vram"] = static_cast<double>(desc.DedicatedVideoMemory) / (1024.0 * 1024.0 * 1024.0);
            m_gpus.append(gpu);
            
            m_vramTotal += gpu["vram"].toDouble();
            pAdapter->Release();
        }
        pFactory->Release();
    }
#else
    m_cpuName = QSysInfo::prettyProductName();
    m_cpuArchitecture = QSysInfo::currentCpuArchitecture();
    m_cpuFlags = "Unknown";
#endif
    // Log detected hardware info
    Logger::info(QStringLiteral("Hardware"), QStringLiteral("Hardware Detection Finished:"));
    Logger::info(QStringLiteral("Hardware"), QStringLiteral("  CPU: %1 (%2)").arg(m_cpuName, m_cpuArchitecture));
    Logger::info(QStringLiteral("Hardware"), QStringLiteral("  CPU Flags: %1").arg(m_cpuFlags));
    Logger::info(QStringLiteral("Hardware"), QStringLiteral("  RAM Total: %1 GB").arg(m_ramTotal, 0, 'f', 2));
    Logger::info(QStringLiteral("Hardware"), QStringLiteral("  GPUs Count: %1, VRAM Total: %2 GB").arg(m_gpus.size()).arg(m_vramTotal, 0, 'f', 2));
    for (int i = 0; i < m_gpus.size(); ++i) {
        QVariantMap gpu = m_gpus.at(i).toMap();
        Logger::info(QStringLiteral("Hardware"), QStringLiteral("    GPU %1: %2 (%3 GB VRAM)").arg(i).arg(gpu["name"].toString()).arg(gpu["vram"].toDouble(), 0, 'f', 2));
    }

    emit hardwareInfoChanged();
}

void HardwareManager::updateResourceUsage() {
    updateCpuUsage();
    updateRamUsage();
    updateVramUsage();
    emit resourceUsageChanged();
}

void HardwareManager::updateCpuUsage() {
#ifdef Q_OS_WIN
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        unsigned long long idle = (static_cast<unsigned long long>(idleTime.dwHighDateTime) << 32) | idleTime.dwLowDateTime;
        unsigned long long kernel = (static_cast<unsigned long long>(kernelTime.dwHighDateTime) << 32) | kernelTime.dwLowDateTime;
        unsigned long long user = (static_cast<unsigned long long>(userTime.dwHighDateTime) << 32) | userTime.dwLowDateTime;

        if (m_lastIdleTime > 0) {
            unsigned long long idleDiff = idle - m_lastIdleTime;
            unsigned long long kernelDiff = kernel - m_lastKernelTime;
            unsigned long long userDiff = user - m_lastUserTime;
            unsigned long long totalDiff = kernelDiff + userDiff;

            if (totalDiff > 0) {
                m_cpuUsage = 100.0 * (totalDiff - idleDiff) / totalDiff;
            }
        }

        m_lastIdleTime = idle;
        m_lastKernelTime = kernel;
        m_lastUserTime = user;
    }
#endif
}

void HardwareManager::updateRamUsage() {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memStatus)) {
        m_ramUsed = static_cast<double>(memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
    }
#endif
}

void HardwareManager::updateVramUsage() {
#ifdef Q_OS_WIN
    m_vramUsed = 0;
    IDXGIFactory1* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory))) {
        IDXGIAdapter1* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);
            
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                pAdapter->Release();
                continue;
            }

            // For DXGI 1.4+ we can get query video memory info
            IDXGIAdapter3* pAdapter3 = nullptr;
            if (SUCCEEDED(pAdapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pAdapter3))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
                if (SUCCEEDED(pAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
                    m_vramUsed += static_cast<double>(memInfo.CurrentUsage) / (1024.0 * 1024.0 * 1024.0);
                }
                pAdapter3->Release();
            }
            pAdapter->Release();
        }
        pFactory->Release();
    }
#endif
}

} // namespace LAStudio
