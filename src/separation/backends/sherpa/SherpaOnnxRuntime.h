#pragma once

#include <QString>
#include <QVector>
#include <QLibrary>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LAStudio {

class SherpaOnnxRuntime final {
public:
    struct SpleeterConfig { const char *vocals; const char *accompaniment; };
    struct UvrConfig { const char *model; };
    struct ModelConfig { SpleeterConfig spleeter; UvrConfig uvr; int numThreads; int debug; const char *provider; };
    struct Config { ModelConfig model; };
    struct Engine;
    struct Stem { float **samples; int numChannels; int n; };
    struct Output { const Stem *stems; int numStems; int sampleRate; };
    
    using CreateFn = const Engine *(*)(const Config *);
    using DestroyFn = void (*)(const Engine *);
    using ProcessFn = const Output *(*)(const Engine *, const float *const *, int, int, int);
    using DestroyOutputFn = void (*)(const Output *);

    explicit SherpaOnnxRuntime(const QString &libraryPath);
    ~SherpaOnnxRuntime();

    bool isLoaded() const { return m_loaded; }
    QString errorString() const { return m_error; }

    const Engine* createEngine(const Config &config) const;
    void destroyEngine(const Engine *engine) const;
    const Output* process(const Engine *engine, const float *const *samples, int numChannels, int samplesPerChannel, int sampleRate) const;
    void destroyOutput(const Output *output) const;

private:
    QLibrary m_library;
    bool m_loaded = false;
    QString m_error;
#ifdef Q_OS_WIN
    QVector<HMODULE> m_preloadedDlls;
#endif

    CreateFn m_create = nullptr;
    DestroyFn m_destroy = nullptr;
    ProcessFn m_process = nullptr;
    DestroyOutputFn m_destroyOutput = nullptr;
};

} // namespace LAStudio
