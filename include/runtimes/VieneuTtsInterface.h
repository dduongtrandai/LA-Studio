#pragma once

#include <QLibrary>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QSet>
#include <QVector>
#include <stdbool.h>
#include <stdint.h>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LAStudio {

struct vieneu_init_params {
    int          abi_version;
    const char * model_path;
    const char * encoder_path;
    const char * decoder_path;
    const char * voices_json_path;
    int          n_ctx;
    int          n_threads;
    int          n_gpu_layers;
    bool         flash_attn;
    bool         mlock;
};

struct vieneu_audio {
    float * samples;
    int     n_samples;
    int     sample_rate;
    int     channels;
};

struct vieneu_context;

struct vieneu_progress {
    int          abi_version;
    const char * stage;
    int          current;
    int          total;
    float        progress;
    const char * message;
};

typedef void (*vieneu_progress_callback)(const struct vieneu_progress * progress, void * user_data);

struct vieneu_tts_params {
    int           abi_version;
    const char *  text;
    const char *  voice_id;
    const float * voice_embedding;
    float         temperature;
    int           top_k;
    int           max_chars;
    int           max_tokens;
    bool          skip_normalize;
    bool          skip_phonemize;
    bool          apply_watermark;
};

struct vieneu_init_params_v2 {
    int          abi_version;
    const char * profile;
    const char * model_dir;
    const char * onnx_dir;
    const char * codec_dir;
    const char * config_path;
    const char * tokenizer_path;
    const char * voices_json_path;
    int          n_threads;
};

struct vieneu_tts_params_v2 {
    int          abi_version;
    const char * text;
    const char * voice_id;
    const char * ref_audio_path;
    float        temperature;
    int          top_k;
    float        top_p;
    int          max_new_frames;
    float        repetition_penalty;
    int          max_chars;
    bool         apply_watermark;
};

struct vieneu_tts_params_v3 {
    int          abi_version;
    const char * text;
    const char * voice_id;
    const char * ref_audio_path;
    const char * style;
    float        temperature;
    int          top_k;
    float        top_p;
    int          max_new_frames;
    float        repetition_penalty;
    int          max_chars;
    bool         denoise_ref;
    bool         use_ref_codes;
    bool         apply_watermark;
};

typedef const char * (*vieneu_version_fn)(void);
typedef const char * (*vieneu_last_error_fn)(void);
typedef void (*vieneu_audio_free_fn)(struct vieneu_audio * a);
typedef void (*vieneu_init_default_params_fn)(struct vieneu_init_params * p);
typedef struct vieneu_context * (*vieneu_init_fn)(const struct vieneu_init_params * params);
typedef void (*vieneu_free_fn)(struct vieneu_context * vieneu);
typedef void (*vieneu_set_progress_callback_fn)(struct vieneu_context * vieneu, vieneu_progress_callback callback, void * user_data);
typedef void (*vieneu_tts_default_params_fn)(struct vieneu_tts_params * p);
typedef int (*vieneu_synthesize_fn)(struct vieneu_context * vieneu, const struct vieneu_tts_params * params, struct vieneu_audio * out);
typedef void (*vieneu_init_v2_default_params_fn)(struct vieneu_init_params_v2 * p);
typedef struct vieneu_context * (*vieneu_init_v2_fn)(const struct vieneu_init_params_v2 * params);
typedef void (*vieneu_tts_v2_default_params_fn)(struct vieneu_tts_params_v2 * p);
typedef int (*vieneu_synthesize_v2_fn)(struct vieneu_context * vieneu, const struct vieneu_tts_params_v2 * params, struct vieneu_audio * out);
typedef void (*vieneu_tts_v3_default_params_fn)(struct vieneu_tts_params_v3 * p);
typedef int (*vieneu_synthesize_v3_fn)(struct vieneu_context * vieneu, const struct vieneu_tts_params_v3 * params, struct vieneu_audio * out);
typedef int (*vieneu_encode_reference_fn)(struct vieneu_context * vieneu, const char * ref_audio_path, float * out_embedding_128);
typedef int (*vieneu_list_preset_voices_fn)(struct vieneu_context * vieneu, char * out_json, int max_len);
typedef int (*vieneu_set_preset_voice_fn)(struct vieneu_context * vieneu, const char * voice_id);

class VieneuTtsInterface {
public:
    static VieneuTtsInterface& instance() {
        static VieneuTtsInterface inst;
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
        vieneu_version = nullptr;
        vieneu_last_error = nullptr;
        vieneu_audio_free = nullptr;
        vieneu_init_default_params = nullptr;
        vieneu_init = nullptr;
        vieneu_free = nullptr;
        vieneu_set_progress_callback = nullptr;
        vieneu_tts_default_params = nullptr;
        vieneu_synthesize = nullptr;
        vieneu_init_v2_default_params = nullptr;
        vieneu_init_v2 = nullptr;
        vieneu_tts_v2_default_params = nullptr;
        vieneu_synthesize_v2 = nullptr;
        vieneu_tts_v3_default_params = nullptr;
        vieneu_synthesize_v3 = nullptr;
        vieneu_encode_reference = nullptr;
        vieneu_list_preset_voices = nullptr;
        vieneu_set_preset_voice = nullptr;
        m_loadedPath.clear();
        m_lastError.clear();
    }

    bool load(const QString& libPath) {
        if (isLoaded()) {
            if (m_loadedPath == libPath) return true;
            m_lastError = QStringLiteral("Hot-switch between VieNeu TTS runtimes is not supported safely in one session. Please restart app after changing runtime.");
            return false;
        }

        bool ok = false;
#ifdef Q_OS_WIN
        const QString cleanLibPath = QDir::toNativeSeparators(QDir::cleanPath(libPath));
        if (!preloadRuntimeDlls(cleanLibPath)) {
            return false;
        }
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
        ok = true;
#else
        QFileInfo fi(libPath);
        QString dir = QDir::toNativeSeparators(fi.absolutePath());

        m_lib.setFileName(libPath);
        ok = m_lib.load();

        if (!ok) {
            m_lastError = m_lib.errorString();
            return false;
        }
#endif

        vieneu_version = resolve<vieneu_version_fn>("vieneu_version");
        vieneu_last_error = resolve<vieneu_last_error_fn>("vieneu_last_error");
        vieneu_audio_free = resolve<vieneu_audio_free_fn>("vieneu_audio_free");
        vieneu_init_default_params = resolve<vieneu_init_default_params_fn>("vieneu_init_default_params");
        vieneu_init = resolve<vieneu_init_fn>("vieneu_init");
        vieneu_free = resolve<vieneu_free_fn>("vieneu_free");
        vieneu_set_progress_callback = resolve<vieneu_set_progress_callback_fn>("vieneu_set_progress_callback");
        vieneu_tts_default_params = resolve<vieneu_tts_default_params_fn>("vieneu_tts_default_params");
        vieneu_synthesize = resolve<vieneu_synthesize_fn>("vieneu_synthesize");
        vieneu_init_v2_default_params = resolve<vieneu_init_v2_default_params_fn>("vieneu_init_v2_default_params");
        vieneu_init_v2 = resolve<vieneu_init_v2_fn>("vieneu_init_v2");
        vieneu_tts_v2_default_params = resolve<vieneu_tts_v2_default_params_fn>("vieneu_tts_v2_default_params");
        vieneu_synthesize_v2 = resolve<vieneu_synthesize_v2_fn>("vieneu_synthesize_v2");
        vieneu_tts_v3_default_params = resolve<vieneu_tts_v3_default_params_fn>("vieneu_tts_v3_default_params");
        vieneu_synthesize_v3 = resolve<vieneu_synthesize_v3_fn>("vieneu_synthesize_v3");
        vieneu_encode_reference = resolve<vieneu_encode_reference_fn>("vieneu_encode_reference");
        vieneu_list_preset_voices = resolve<vieneu_list_preset_voices_fn>("vieneu_list_preset_voices");
        vieneu_set_preset_voice = resolve<vieneu_set_preset_voice_fn>("vieneu_set_preset_voice");

        ok = hasCommonAbi() && (hasAbiV1() || hasAbiV2());

        if (!ok) {
            m_lastError = QStringLiteral("Failed to resolve VieNeu TTS runtime symbols from %1. Missing: %2")
                              .arg(libPath, missingSymbols().join(QStringLiteral(", ")));
            unloadLoadedLibraryOnly();
        } else {
            m_loadedPath = libPath;
            m_lastError.clear();
        }
        return ok;
    }

    bool isLoaded() const {
#ifdef Q_OS_WIN
        return m_module != nullptr;
#else
        return m_lib.isLoaded();
#endif
    }
    bool hasCommonAbi() const { return vieneu_version && vieneu_last_error && vieneu_audio_free && vieneu_free; }
    bool hasAbiV1() const {
        return vieneu_init_default_params && vieneu_init &&
               vieneu_tts_default_params && vieneu_synthesize &&
               vieneu_encode_reference && vieneu_list_preset_voices && vieneu_set_preset_voice;
    }
    bool hasAbiV2() const {
        return vieneu_init_v2_default_params && vieneu_init_v2 &&
               vieneu_tts_v2_default_params && vieneu_synthesize_v2;
    }
    bool hasAbiV3() const {
        return hasAbiV2() && vieneu_tts_v3_default_params && vieneu_synthesize_v3;
    }
    QString errorString() const {
        if (!m_lastError.isEmpty()) return m_lastError;
#ifdef Q_OS_WIN
        return QStringLiteral("VieNeu TTS runtime is not loaded.");
#else
        return m_lib.errorString();
#endif
    }

    vieneu_version_fn vieneu_version = nullptr;
    vieneu_last_error_fn vieneu_last_error = nullptr;
    vieneu_audio_free_fn vieneu_audio_free = nullptr;
    vieneu_init_default_params_fn vieneu_init_default_params = nullptr;
    vieneu_init_fn vieneu_init = nullptr;
    vieneu_free_fn vieneu_free = nullptr;
    vieneu_set_progress_callback_fn vieneu_set_progress_callback = nullptr;
    vieneu_tts_default_params_fn vieneu_tts_default_params = nullptr;
    vieneu_synthesize_fn vieneu_synthesize = nullptr;
    vieneu_init_v2_default_params_fn vieneu_init_v2_default_params = nullptr;
    vieneu_init_v2_fn vieneu_init_v2 = nullptr;
    vieneu_tts_v2_default_params_fn vieneu_tts_v2_default_params = nullptr;
    vieneu_synthesize_v2_fn vieneu_synthesize_v2 = nullptr;
    vieneu_tts_v3_default_params_fn vieneu_tts_v3_default_params = nullptr;
    vieneu_synthesize_v3_fn vieneu_synthesize_v3 = nullptr;
    vieneu_encode_reference_fn vieneu_encode_reference = nullptr;
    vieneu_list_preset_voices_fn vieneu_list_preset_voices = nullptr;
    vieneu_set_preset_voice_fn vieneu_set_preset_voice = nullptr;

private:
    VieneuTtsInterface() = default;
    ~VieneuTtsInterface() { unload(); }
    VieneuTtsInterface(const VieneuTtsInterface&) = delete;
    VieneuTtsInterface& operator=(const VieneuTtsInterface&) = delete;

    template<typename Fn, typename... Names>
    Fn resolve(const char *primary, Names... names) {
#ifdef Q_OS_WIN
        if (m_module) {
            if (auto fn = reinterpret_cast<Fn>(GetProcAddress(m_module, primary))) {
                return fn;
            }
        }
#else
        if (auto fn = reinterpret_cast<Fn>(m_lib.resolve(primary))) {
            return fn;
        }
#endif
        if constexpr (sizeof...(names) == 0) {
            return nullptr;
        } else {
            return resolve<Fn>(names...);
        }
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
        if (name == QStringLiteral("ggml.dll") || name == QStringLiteral("ggmk.dll")) return 0;
        if (name == QStringLiteral("ggml-base.dll") || name == QStringLiteral("ggmk-base.dll")) return 1;
        if (name.startsWith(QStringLiteral("ggml-")) || name.startsWith(QStringLiteral("ggmk-"))) return 2;
        if (name.startsWith(QStringLiteral("onnxruntime"))) return 3;
        if (name.startsWith(QStringLiteral("cudart")) || name.startsWith(QStringLiteral("cublas"))) return 4;
        return 5;
    }

    static QString loadedModulePath(HMODULE module) {
        if (!module) return QString();
        std::wstring buffer(32768, L'\0');
        const DWORD length = GetModuleFileNameW(module, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size()) return QString();
        buffer.resize(length);
        return QDir::toNativeSeparators(
            QFileInfo(QString::fromStdWString(buffer)).absoluteFilePath());
    }

    void setRuntimeDependencyConflict(const QString &requestedPath,
                                      const QString &loadedPath) {
        m_lastError = QStringLiteral(
            "Cannot safely load the VieNeu TTS runtime because dependency %1 "
            "is already loaded from %2. Unload the other AI runtime first, "
            "then load VieNeu again.")
            .arg(QFileInfo(requestedPath).fileName(), loadedPath);
    }

    bool preloadRuntimeDlls(const QString &mainLibPath) {
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

            const std::wstring fileName = dll.fileName().toStdWString();
            if (HMODULE existing = GetModuleHandleW(fileName.c_str())) {
                const QString existingPath = loadedModulePath(existing);
                if (!existingPath.isEmpty()
                    && QFileInfo(existingPath).absoluteFilePath().compare(
                           QFileInfo(dllPath).absoluteFilePath(),
                           Qt::CaseInsensitive) != 0) {
                    releasePreloadedDlls();
                    setRuntimeDependencyConflict(dllPath, existingPath);
                    return false;
                }
            }

            HMODULE module = LoadLibraryExW(reinterpret_cast<LPCWSTR>(dllPath.utf16()),
                                            NULL,
                                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (module) {
                const QString actualPath = loadedModulePath(module);
                if (!actualPath.isEmpty()
                    && QFileInfo(actualPath).absoluteFilePath().compare(
                           QFileInfo(dllPath).absoluteFilePath(),
                           Qt::CaseInsensitive) != 0) {
                    FreeLibrary(module);
                    releasePreloadedDlls();
                    setRuntimeDependencyConflict(dllPath, actualPath);
                    return false;
                }
                m_preloadedDlls.append(module);
            }
        }
        return true;
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
        if (errorCode == ERROR_PROC_NOT_FOUND) {
            detail += QStringLiteral(" This usually means a dependent DLL with the same name was found but does not export the procedure required by this VieNeu runtime. Restart LA Studio and reinstall the matching VieNeu runtime package if the problem persists.");
        }
        return QStringLiteral("Cannot load library %1: %2").arg(libPath, detail);
    }
#endif

    QStringList missingSymbols() const {
        QStringList missing;
        appendIfMissing(missing, vieneu_version, QStringLiteral("vieneu_version"));
        appendIfMissing(missing, vieneu_last_error, QStringLiteral("vieneu_last_error"));
        appendIfMissing(missing, vieneu_audio_free, QStringLiteral("vieneu_audio_free"));
        appendIfMissing(missing, vieneu_free, QStringLiteral("vieneu_free"));

        if (!hasAbiV1() && !hasAbiV2()) {
            appendIfMissing(missing, vieneu_init_default_params, QStringLiteral("vieneu_init_default_params"));
            appendIfMissing(missing, vieneu_init, QStringLiteral("vieneu_init"));
            appendIfMissing(missing, vieneu_tts_default_params, QStringLiteral("vieneu_tts_default_params"));
            appendIfMissing(missing, vieneu_synthesize, QStringLiteral("vieneu_synthesize"));
            appendIfMissing(missing, vieneu_init_v2_default_params, QStringLiteral("vieneu_init_v2_default_params"));
            appendIfMissing(missing, vieneu_init_v2, QStringLiteral("vieneu_init_v2"));
            appendIfMissing(missing, vieneu_tts_v2_default_params, QStringLiteral("vieneu_tts_v2_default_params"));
            appendIfMissing(missing, vieneu_synthesize_v2, QStringLiteral("vieneu_synthesize_v2"));
        }

        if (missing.isEmpty()) {
            missing << QStringLiteral("no complete ABI v1 or ABI v2 symbol set");
        }
        return missing;
    }

    template<typename Fn>
    static void appendIfMissing(QStringList& missing, Fn fn, const QString& symbol) {
        if (!fn) {
            missing << symbol;
        }
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
