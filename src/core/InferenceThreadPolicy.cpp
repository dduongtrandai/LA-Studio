#include "InferenceThreadPolicy.h"

#include <QThread>
#include <QtGlobal>
#include <QByteArray>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace LAStudio {

namespace {

struct ThreadBounds {
    int minThreads;
    int maxThreads;
    int reservedThreads;
    double loadFactor;
};

ThreadBounds boundsFor(InferenceBackendProfile profile)
{
    switch (profile) {
    case InferenceBackendProfile::WhisperCpu:
        return {4, 16, 1, 1.00};
    case InferenceBackendProfile::WhisperGpu:
        return {2, 8, 1, 0.50};
    case InferenceBackendProfile::CrispAsrCpu:
        return {4, 16, 1, 1.00};
    case InferenceBackendProfile::CrispAsrGpu:
        return {2, 8, 1, 0.50};
    case InferenceBackendProfile::VieNeuNative:
        return {4, 12, 1, 1.00};
    case InferenceBackendProfile::Kokoro:
        return {2, 8, 1, 0.75};
    case InferenceBackendProfile::RealtimeTts:
        return {2, 4, 1, 0.60};
    }
    return {4, 16, 1, 1.00};
}

int fallbackEffectiveCoreCount(int logical)
{
    if (logical <= 0) {
        return 8;
    }
    if (logical <= 4) {
        return logical;
    }
    return std::max(1, static_cast<int>(std::lround(logical * 0.75)));
}

#ifdef Q_OS_WIN
int windowsPhysicalCoreCount()
{
    DWORD length = 0;
    if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
        length == 0) {
        return 0;
    }

    QByteArray buffer(static_cast<int>(length), Qt::Uninitialized);
    auto *info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &length)) {
        return 0;
    }

    int cores = 0;
    char *cursor = buffer.data();
    char *end = cursor + length;
    while (cursor < end) {
        auto *entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(cursor);
        if (entry->Relationship == RelationProcessorCore) {
            ++cores;
        }
        if (entry->Size == 0) {
            break;
        }
        cursor += entry->Size;
    }
    return cores;
}
#endif

} // namespace

int InferenceThreadPolicy::logicalThreadCount()
{
    const int ideal = QThread::idealThreadCount();
    return ideal > 0 ? ideal : 8;
}

int InferenceThreadPolicy::effectiveCoreCount()
{
    const int logical = logicalThreadCount();
#ifdef Q_OS_WIN
    const int physical = windowsPhysicalCoreCount();
    if (physical > 0) {
        return std::min(physical, logical);
    }
#endif
    return fallbackEffectiveCoreCount(logical);
}

int InferenceThreadPolicy::recommendedThreadCount(const InferenceThreadRequest &request)
{
    const ThreadBounds bounds = boundsFor(request.profile);
    if (request.requestedThreads > 0) {
        return qBound(1, request.requestedThreads, 64);
    }

    const int jobs = std::max(1, request.activeInferenceJobs);
    const int reserved = std::max(0, bounds.reservedThreads);
    const int available = std::max(1, effectiveCoreCount() - reserved);
    const int perJob = std::max(1, available / jobs);
    const int scaled = std::max(1, static_cast<int>(std::lround(perJob * bounds.loadFactor)));
    return qBound(bounds.minThreads, scaled, bounds.maxThreads);
}

int InferenceThreadPolicy::recommendedThreadCount(InferenceBackendProfile profile, int requestedThreads)
{
    return recommendedThreadCount({profile, requestedThreads, 1});
}

QString InferenceThreadPolicy::describeProfile(InferenceBackendProfile profile)
{
    switch (profile) {
    case InferenceBackendProfile::WhisperCpu:
        return QStringLiteral("Whisper CPU");
    case InferenceBackendProfile::WhisperGpu:
        return QStringLiteral("Whisper GPU");
    case InferenceBackendProfile::CrispAsrCpu:
        return QStringLiteral("CrispASR CPU");
    case InferenceBackendProfile::CrispAsrGpu:
        return QStringLiteral("CrispASR GPU");
    case InferenceBackendProfile::VieNeuNative:
        return QStringLiteral("VieNeu native");
    case InferenceBackendProfile::Kokoro:
        return QStringLiteral("Kokoro");
    case InferenceBackendProfile::RealtimeTts:
        return QStringLiteral("Realtime TTS");
    }
    return QStringLiteral("Inference");
}

} // namespace LAStudio
