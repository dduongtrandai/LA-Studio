#pragma once

#include <QLibrary>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <stdbool.h>
#include <stdint.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LAStudio {

typedef struct vv_context vv_context;
typedef struct vv_voice vv_voice;

struct vv_context_params {
    const char* model_path;
    int         n_threads;
    int         gpu_layers;
    bool        use_f16_kv;
    uint32_t    seed;
};

struct vv_audio {
    float*  samples;
    size_t  n_samples;
    int     sample_rate;
    int     channels;
};

struct vv_tts_params {
    const vv_voice* voice;
    int             n_diffusion_steps;
    float           cfg_scale;
    int             max_new_tokens;
    uint32_t        seed;
};

class VibevoiceInterface {
public:
    static VibevoiceInterface& instance() {
        static VibevoiceInterface inst;
        return inst;
    }

    void unload() {
        if (m_lib.isLoaded()) {
            m_lib.unload();
        }
        vv_version = nullptr;
        vv_context_default_params = nullptr;
        vv_init = nullptr;
        vv_free = nullptr;
        vv_audio_free = nullptr;
        vv_voice_load = nullptr;
        vv_voice_free = nullptr;
        vv_tts_default_params = nullptr;
        vv_tts = nullptr;
        vv_capi_load = nullptr;
        vv_capi_tts = nullptr;
        vv_capi_unload = nullptr;
        vv_capi_version = nullptr;
    }

    bool load(const QString& libPath) {
        if (m_lib.isLoaded()) {
            if (m_lib.fileName() == libPath) return true;
            m_lib.unload();
        }

        QFileInfo fi(libPath);
        QString dir = QDir::toNativeSeparators(fi.absolutePath());

#ifdef Q_OS_WIN
        SetDllDirectoryW((LPCWSTR)dir.utf16());
#endif

        m_lib.setFileName(libPath);
        bool ok = m_lib.load();

#ifdef Q_OS_WIN
        SetDllDirectoryW(NULL);
#endif

        if (!ok) return false;

        // Resolve symbols (Original API)
        vv_version = (vv_version_fn)m_lib.resolve("vv_version");
        vv_context_default_params = (vv_context_default_params_fn)m_lib.resolve("vv_context_default_params");
        vv_init = (vv_init_fn)m_lib.resolve("vv_init");
        vv_free = (vv_free_fn)m_lib.resolve("vv_free");
        vv_audio_free = (vv_audio_free_fn)m_lib.resolve("vv_audio_free");
        vv_voice_load = (vv_voice_load_fn)m_lib.resolve("vv_voice_load");
        vv_voice_free = (vv_voice_free_fn)m_lib.resolve("vv_voice_free");
        vv_tts_default_params = (vv_tts_default_params_fn)m_lib.resolve("vv_tts_default_params");
        vv_tts = (vv_tts_fn)m_lib.resolve("vv_tts");

        // Resolve CAPI symbols (Stable integration API)
        vv_capi_load = (vv_capi_load_fn)m_lib.resolve("vv_capi_load");
        vv_capi_tts = (vv_capi_tts_fn)m_lib.resolve("vv_capi_tts");
        vv_capi_unload = (vv_capi_unload_fn)m_lib.resolve("vv_capi_unload");
        vv_capi_version = (vv_capi_version_fn)m_lib.resolve("vv_capi_version");

        // We consider it "okay" if we have at least the CAPI load and tts
        ok = vv_capi_load && vv_capi_tts;

        if (!ok) m_lib.unload();
        return ok;
    }

    bool isLoaded() const { return m_lib.isLoaded(); }
    QString errorString() const { return m_lib.errorString(); }

    // Original Function pointers
    typedef const char* (*vv_version_fn)(void);
    typedef struct vv_context_params (*vv_context_default_params_fn)(void);
    typedef vv_context* (*vv_init_fn)(const vv_context_params*);
    typedef void (*vv_free_fn)(vv_context*);
    typedef void (*vv_audio_free_fn)(vv_audio*);
    typedef vv_voice* (*vv_voice_load_fn)(vv_context*, const char*);
    typedef void (*vv_voice_free_fn)(vv_voice*);
    typedef struct vv_tts_params (*vv_tts_default_params_fn)(void);
    typedef int (*vv_tts_fn)(vv_context*, const char*, const vv_tts_params*, vv_audio*);

    // CAPI Function pointers
    typedef int (*vv_capi_load_fn)(const char* tts_model_path, const char* asr_model_path, const char* tokenizer_path, const char* voice_path, int n_threads);
    typedef int (*vv_capi_tts_fn)(const char* text, const char* voice_path, const char* const* ref_audio_paths, int n_ref_audio_paths, const char* dst_wav_path, int n_diffusion_steps, float cfg_scale, int max_speech_frames, uint32_t seed);
    typedef void (*vv_capi_unload_fn)(void);
    typedef const char* (*vv_capi_version_fn)(void);

    vv_version_fn vv_version = nullptr;
    vv_context_default_params_fn vv_context_default_params = nullptr;
    vv_init_fn vv_init = nullptr;
    vv_free_fn vv_free = nullptr;
    vv_audio_free_fn vv_audio_free = nullptr;
    vv_voice_load_fn vv_voice_load = nullptr;
    vv_voice_free_fn vv_voice_free = nullptr;
    vv_tts_default_params_fn vv_tts_default_params = nullptr;
    vv_tts_fn vv_tts = nullptr;

    vv_capi_load_fn vv_capi_load = nullptr;
    vv_capi_tts_fn vv_capi_tts = nullptr;
    vv_capi_unload_fn vv_capi_unload = nullptr;
    vv_capi_version_fn vv_capi_version = nullptr;

private:
    VibevoiceInterface() = default;
    QLibrary m_lib;
};

} // namespace LAStudio
