#pragma once

#include <QLibrary>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QSet>
#include <QVector>
#include <stdint.h>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LAStudio {

struct kokoro_vi_context;

struct kokoro_vi_init_params {
    int         abi_version;
    const char *model_dir;
    const char *onnx_path;
    const char *config_path;
    const char *voicepack_dir;
    const char *g2p_exe_path;
    int         n_threads;
};

struct kokoro_vi_tts_params {
    int         abi_version;
    const char *text;
    const char *voice_id;
    float       speed;
    int         crossfade_ms;
    int         max_phonemes;
};

struct kokoro_vi_audio {
    float *samples;
    int    n_samples;
    int    sample_rate;
    int    channels;
};

typedef const char *(*kokoro_vi_version_fn)(void);
typedef const char *(*kokoro_vi_last_error_fn)(void);
typedef void (*kokoro_vi_init_default_params_fn)(struct kokoro_vi_init_params *p);
typedef struct kokoro_vi_context *(*kokoro_vi_init_fn)(const struct kokoro_vi_init_params *p);
typedef void (*kokoro_vi_free_fn)(struct kokoro_vi_context *ctx);
typedef void (*kokoro_vi_tts_default_params_fn)(struct kokoro_vi_tts_params *p);
typedef int (*kokoro_vi_synthesize_fn)(struct kokoro_vi_context *ctx, const struct kokoro_vi_tts_params *p, struct kokoro_vi_audio *out);
typedef void (*kokoro_vi_audio_free_fn)(struct kokoro_vi_audio *audio);

class KokoroVietnameseInterface {
public:
    static KokoroVietnameseInterface& instance() {
        static KokoroVietnameseInterface inst;
        return inst;
    }

    void unload() {
#ifdef Q_OS_WIN
        if (m_module) {
            FreeLibrary(m_module);
            m_module = nullptr;
        }
        releasePreloadedDlls();
#else
        if (m_lib.isLoaded()) {
            m_lib.unload();
        }
#endif
        kokoro_vi_version = nullptr;
        kokoro_vi_last_error = nullptr;
        kokoro_vi_init_default_params = nullptr;
        kokoro_vi_init = nullptr;
        kokoro_vi_free = nullptr;
        kokoro_vi_tts_default_params = nullptr;
        kokoro_vi_synthesize = nullptr;
        kokoro_vi_audio_free = nullptr;
        m_loadedPath.clear();
        m_lastError.clear();
    }

    bool load(const QString &libPath) {
        if (isLoaded()) {
            if (m_loadedPath == libPath) return true;
            m_lastError = QStringLiteral("Hot-switch between Kokoro-Vietnamese runtimes is not supported safely in one session. Please restart app after changing runtime.");
            return false;
        }

#ifdef Q_OS_WIN
        const QString cleanLibPath = QDir::toNativeSeparators(QDir::cleanPath(libPath));
        preloadRuntimeDlls(cleanLibPath);
        m_module = LoadLibraryExW(
            reinterpret_cast<LPCWSTR>(cleanLibPath.utf16()),
            NULL,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!m_module) {
            const DWORD errorCode = GetLastError();
            releasePreloadedDlls();
            m_lastError = windowsLoadError(libPath, errorCode);
            return false;
        }
#else
        m_lib.setFileName(libPath);
        if (!m_lib.load()) {
            m_lastError = m_lib.errorString();
            return false;
        }
#endif

        kokoro_vi_version = resolve<kokoro_vi_version_fn>("kokoro_vi_version");
        kokoro_vi_last_error = resolve<kokoro_vi_last_error_fn>("kokoro_vi_last_error");
        kokoro_vi_init_default_params = resolve<kokoro_vi_init_default_params_fn>("kokoro_vi_init_default_params");
        kokoro_vi_init = resolve<kokoro_vi_init_fn>("kokoro_vi_init");
        kokoro_vi_free = resolve<kokoro_vi_free_fn>("kokoro_vi_free");
        kokoro_vi_tts_default_params = resolve<kokoro_vi_tts_default_params_fn>("kokoro_vi_tts_default_params");
        kokoro_vi_synthesize = resolve<kokoro_vi_synthesize_fn>("kokoro_vi_synthesize");
        kokoro_vi_audio_free = resolve<kokoro_vi_audio_free_fn>("kokoro_vi_audio_free");

        if (!hasAbi()) {
            m_lastError = QStringLiteral("Failed to resolve Kokoro-Vietnamese runtime symbols from %1. Missing: %2")
                              .arg(libPath, missingSymbols().join(QStringLiteral(", ")));
            unloadLoadedLibraryOnly();
            return false;
        }

        m_loadedPath = libPath;
        m_lastError.clear();
        return true;
    }

    bool isLoaded() const {
#ifdef Q_OS_WIN
        return m_module != nullptr;
#else
        return m_lib.isLoaded();
#endif
    }

    bool hasAbi() const {
        return kokoro_vi_version && kokoro_vi_last_error &&
               kokoro_vi_init_default_params && kokoro_vi_init && kokoro_vi_free &&
               kokoro_vi_tts_default_params && kokoro_vi_synthesize && kokoro_vi_audio_free;
    }

    QString errorString() const {
        if (!m_lastError.isEmpty()) return m_lastError;
#ifdef Q_OS_WIN
        return QStringLiteral("Kokoro-Vietnamese runtime is not loaded.");
#else
        return m_lib.errorString();
#endif
    }

    kokoro_vi_version_fn kokoro_vi_version = nullptr;
    kokoro_vi_last_error_fn kokoro_vi_last_error = nullptr;
    kokoro_vi_init_default_params_fn kokoro_vi_init_default_params = nullptr;
    kokoro_vi_init_fn kokoro_vi_init = nullptr;
    kokoro_vi_free_fn kokoro_vi_free = nullptr;
    kokoro_vi_tts_default_params_fn kokoro_vi_tts_default_params = nullptr;
    kokoro_vi_synthesize_fn kokoro_vi_synthesize = nullptr;
    kokoro_vi_audio_free_fn kokoro_vi_audio_free = nullptr;

private:
    KokoroVietnameseInterface() = default;
    ~KokoroVietnameseInterface() { unload(); }
    KokoroVietnameseInterface(const KokoroVietnameseInterface&) = delete;
    KokoroVietnameseInterface& operator=(const KokoroVietnameseInterface&) = delete;

    template<typename Fn>
    Fn resolve(const char *name) {
#ifdef Q_OS_WIN
        if (m_module) {
            return reinterpret_cast<Fn>(GetProcAddress(m_module, name));
        }
        return nullptr;
#else
        return reinterpret_cast<Fn>(m_lib.resolve(name));
#endif
    }

    void unloadLoadedLibraryOnly() {
#ifdef Q_OS_WIN
        if (m_module) {
            FreeLibrary(m_module);
            m_module = nullptr;
        }
        releasePreloadedDlls();
#else
        if (m_lib.isLoaded()) {
            m_lib.unload();
        }
#endif
    }

#ifdef Q_OS_WIN
    static int dllLoadPriority(const QString &fileName) {
        const QString name = fileName.toLower();
        if (name.startsWith(QStringLiteral("onnxruntime"))) return 0;
        if (name == QStringLiteral("kokoro-vietnamese.dll")) return 10;
        return 5;
    }

    void preloadRuntimeDlls(const QString &mainLibPath) {
        releasePreloadedDlls();

        const QFileInfo mainInfo(mainLibPath);
        const QString mainPath = QDir::toNativeSeparators(QDir::cleanPath(mainInfo.absoluteFilePath()));
        const QDir runtimeDir(mainInfo.absolutePath());
        QVector<QFileInfo> dlls;

        QDirIterator it(runtimeDir.absolutePath(),
                        QStringList{QStringLiteral("*.dll")},
                        QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFileInfo dllInfo(it.next());
            const QString dllPath = QDir::toNativeSeparators(QDir::cleanPath(dllInfo.absoluteFilePath()));
            if (dllPath.compare(mainPath, Qt::CaseInsensitive) == 0) {
                continue;
            }
            dlls.append(dllInfo);
        }

        std::sort(dlls.begin(), dlls.end(), [](const QFileInfo &a, const QFileInfo &b) {
            const int ap = dllLoadPriority(a.fileName());
            const int bp = dllLoadPriority(b.fileName());
            if (ap != bp) return ap < bp;
            return a.fileName().compare(b.fileName(), Qt::CaseInsensitive) < 0;
        });

        QSet<QString> loadedPaths;
        for (const QFileInfo &dll : dlls) {
            const QString dllPath = QDir::toNativeSeparators(QDir::cleanPath(dll.absoluteFilePath()));
            if (loadedPaths.contains(dllPath)) {
                continue;
            }
            loadedPaths.insert(dllPath);

            HMODULE module = LoadLibraryExW(reinterpret_cast<LPCWSTR>(dllPath.utf16()),
                                            NULL,
                                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (module) {
                m_preloadedDlls.append(module);
            }
        }
    }

    void releasePreloadedDlls() {
        for (auto it = m_preloadedDlls.rbegin(); it != m_preloadedDlls.rend(); ++it) {
            FreeLibrary(*it);
        }
        m_preloadedDlls.clear();
    }

    static QString windowsLoadError(const QString &libPath, DWORD errorCode) {
        LPWSTR message = nullptr;
        const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS;
        const DWORD len = FormatMessageW(flags,
                                         NULL,
                                         errorCode,
                                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                         reinterpret_cast<LPWSTR>(&message),
                                         0,
                                         NULL);
        QString detail = len > 0 && message
            ? QString::fromWCharArray(message).trimmed()
            : QStringLiteral("Windows error %1").arg(errorCode);
        if (message) {
            LocalFree(message);
        }
        return QStringLiteral("Cannot load library %1: %2").arg(libPath, detail);
    }
#endif

    QStringList missingSymbols() const {
        QStringList missing;
        appendIfMissing(missing, kokoro_vi_version, QStringLiteral("kokoro_vi_version"));
        appendIfMissing(missing, kokoro_vi_last_error, QStringLiteral("kokoro_vi_last_error"));
        appendIfMissing(missing, kokoro_vi_init_default_params, QStringLiteral("kokoro_vi_init_default_params"));
        appendIfMissing(missing, kokoro_vi_init, QStringLiteral("kokoro_vi_init"));
        appendIfMissing(missing, kokoro_vi_free, QStringLiteral("kokoro_vi_free"));
        appendIfMissing(missing, kokoro_vi_tts_default_params, QStringLiteral("kokoro_vi_tts_default_params"));
        appendIfMissing(missing, kokoro_vi_synthesize, QStringLiteral("kokoro_vi_synthesize"));
        appendIfMissing(missing, kokoro_vi_audio_free, QStringLiteral("kokoro_vi_audio_free"));
        return missing;
    }

    template<typename Fn>
    static void appendIfMissing(QStringList &missing, Fn fn, const QString &symbol) {
        if (!fn) missing << symbol;
    }

#ifdef Q_OS_WIN
    HMODULE m_module = nullptr;
    QVector<HMODULE> m_preloadedDlls;
#else
    QLibrary m_lib;
#endif
    QString m_loadedPath;
    QString m_lastError;
};

} // namespace LAStudio
