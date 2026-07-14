#include "SherpaOnnxRuntime.h"
#include <QFileInfo>
#include <QDir>
#include <runtimes/CrispCommon.h>

namespace LAStudio {

SherpaOnnxRuntime::SherpaOnnxRuntime(const QString &libraryPath)
{
    if (libraryPath.isEmpty() || !QFileInfo::exists(libraryPath)) {
        m_error = QStringLiteral("sherpa-onnx source-separation runtime was not found.");
        return;
    }
    
    const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(libraryPath));
    const QStringList runtimeDirs = crispRuntimeDependencyDirs(cleanPath);
    crispPrependRuntimeDirsToPath(runtimeDirs);
    
#ifdef Q_OS_WIN
    m_preloadedDlls = crispPreloadRuntimeDlls(cleanPath, runtimeDirs);
#endif

    m_library.setFileName(cleanPath);
    m_library.setLoadHints(QLibrary::ExportExternalSymbolsHint);
    
    if (!m_library.load()) {
        m_error = QStringLiteral("Failed to load sherpa-onnx runtime: %1").arg(m_library.errorString());
#ifdef Q_OS_WIN
        crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
#endif
        return;
    }
    
    m_create = reinterpret_cast<CreateFn>(m_library.resolve("SherpaOnnxCreateOfflineSourceSeparation"));
    m_destroy = reinterpret_cast<DestroyFn>(m_library.resolve("SherpaOnnxDestroyOfflineSourceSeparation"));
    m_process = reinterpret_cast<ProcessFn>(m_library.resolve("SherpaOnnxOfflineSourceSeparationProcess"));
    m_destroyOutput = reinterpret_cast<DestroyOutputFn>(m_library.resolve("SherpaOnnxDestroySourceSeparationOutput"));
    
    if (!m_create || !m_destroy || !m_process || !m_destroyOutput) {
        m_error = QStringLiteral("sherpa-onnx runtime is missing source-separation C API symbols.");
        m_library.unload();
#ifdef Q_OS_WIN
        crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
#endif
        m_create = nullptr;
        m_destroy = nullptr;
        m_process = nullptr;
        m_destroyOutput = nullptr;
        return;
    }
    
    m_loaded = true;
}

SherpaOnnxRuntime::~SherpaOnnxRuntime()
{
    if (m_library.isLoaded()) {
        m_library.unload();
    }
#ifdef Q_OS_WIN
    crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
#endif
}

const SherpaOnnxRuntime::Engine* SherpaOnnxRuntime::createEngine(const Config &config) const
{
    if (!m_loaded || !m_create) return nullptr;
    return m_create(&config);
}

void SherpaOnnxRuntime::destroyEngine(const Engine *engine) const
{
    if (m_loaded && m_destroy && engine) {
        m_destroy(engine);
    }
}

const SherpaOnnxRuntime::Output* SherpaOnnxRuntime::process(
    const Engine *engine, const float *const *samples, int numChannels, int samplesPerChannel, int sampleRate) const
{
    if (!m_loaded || !m_process || !engine) return nullptr;
    return m_process(engine, samples, numChannels, samplesPerChannel, sampleRate);
}

void SherpaOnnxRuntime::destroyOutput(const Output *output) const
{
    if (m_loaded && m_destroyOutput && output) {
        m_destroyOutput(output);
    }
}

} // namespace LAStudio
