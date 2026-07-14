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

// Re-declare needed structs from omnivoice.h to avoid header dependency if possible,
// but since we already have the folder, we can just include it if we want.
// However, for a "Pluggable Runtime", it's better to define the types here.

enum ov_status {
    OV_STATUS_OK               = 0,
    OV_STATUS_INVALID_PARAMS   = -1,
    OV_STATUS_INSTRUCT_INVALID = -2,
    OV_STATUS_GENERATE_FAILED  = -3,
    OV_STATUS_OOM              = -4,
    OV_STATUS_CANCELLED        = -5,
};

struct ov_audio {
    float * samples;
    int     n_samples;
    int     sample_rate;
    int     channels;
};

struct ov_context;

struct ov_init_params {
    int          abi_version;
    const char * model_path;
    const char * codec_path;
    bool         use_fa;
    bool         clamp_fp16;
};

typedef bool (*ov_cancel_cb)(void * user_data);
typedef bool (*ov_audio_chunk_cb)(const float * samples, int n_samples, void * user_data);
typedef bool (*ov_progress_cb)(int current, int total, const char * stage, int chunk_index, int chunk_count, void * user_data);
typedef const char * (*ov_version_fn)(void);
typedef const char * (*ov_last_error_fn)(void);
typedef void (*ov_audio_free_fn)(struct ov_audio * a);
typedef void (*ov_init_default_params_fn)(struct ov_init_params * p);
typedef struct ov_context * (*ov_init_fn)(const struct ov_init_params * params);
typedef void (*ov_free_fn)(struct ov_context * ov);
typedef void (*ov_tts_default_params_fn)(struct ov_tts_params * p);
typedef enum ov_status (*ov_synthesize_fn)(struct ov_context * ov, const struct ov_tts_params * params, struct ov_audio * out);
typedef int (*ov_duration_sec_to_tokens_fn)(const struct ov_context * ov, float duration_sec);

struct ov_tts_params {
    int abi_version;
    const char * text;
    const char * lang;
    const char * instruct;
    int   T_override;
    float chunk_duration_sec;
    float chunk_threshold_sec;
    bool denoise;
    bool preprocess_prompt;
    int      mg_num_step;
    float    mg_guidance_scale;
    float    mg_t_shift;
    float    mg_layer_penalty_factor;
    float    mg_position_temperature;
    float    mg_class_temperature;
    uint64_t mg_seed;
    const int32_t * ref_audio_tokens;
    int             ref_T;
    const float *   ref_audio_24k;
    int             ref_n_samples;
    const char *    ref_text;
    const char * dump_dir;
    ov_cancel_cb cancel;
    void *       cancel_user_data;
    ov_audio_chunk_cb on_chunk;
    void *            on_chunk_user_data;
    bool postproc;
    ov_progress_cb on_progress;
    void *         on_progress_user_data;
};

class OmnivoiceInterface {
public:
    static OmnivoiceInterface& instance() {
        static OmnivoiceInterface inst;
        return inst;
    }

    bool unload() {
        if (m_lib.isLoaded()) {
            if (!m_lib.unload()) {
                m_lastError = m_lib.errorString();
                return false;
            }
        }
        ov_version = nullptr;
        ov_last_error = nullptr;
        ov_audio_free = nullptr;
        ov_init_default_params = nullptr;
        ov_init = nullptr;
        ov_free = nullptr;
        ov_tts_default_params = nullptr;
        ov_synthesize = nullptr;
        ov_duration_sec_to_tokens = nullptr;
        m_loadedPath.clear();
        m_lastError.clear();
        return true;
    }

    bool load(const QString& libPath) {
        if (m_lib.isLoaded()) {
            if (m_lib.fileName() == libPath) return true;
            m_lastError = QStringLiteral("Hot-switch between OmniVoice runtimes is not supported safely in one session. Please restart app after changing runtime.");
            return false;
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

        if (!ok) {
            m_lastError = m_lib.errorString();
            return false;
        }

        // Resolve symbols
        ov_version = (ov_version_fn)m_lib.resolve("ov_version");
        ov_last_error = (ov_last_error_fn)m_lib.resolve("ov_last_error");
        ov_audio_free = (ov_audio_free_fn)m_lib.resolve("ov_audio_free");
        ov_init_default_params = (ov_init_default_params_fn)m_lib.resolve("ov_init_default_params");
        ov_init = (ov_init_fn)m_lib.resolve("ov_init");
        ov_free = (ov_free_fn)m_lib.resolve("ov_free");
        ov_tts_default_params = (ov_tts_default_params_fn)m_lib.resolve("ov_tts_default_params");
        ov_synthesize = (ov_synthesize_fn)m_lib.resolve("ov_synthesize");
        ov_duration_sec_to_tokens = (ov_duration_sec_to_tokens_fn)m_lib.resolve("ov_duration_sec_to_tokens");

        ok = ov_version && ov_last_error && ov_audio_free && 
             ov_init_default_params && ov_init && ov_free && 
             ov_tts_default_params && ov_synthesize && ov_duration_sec_to_tokens;
        
        if (!ok) {
            m_lastError = QStringLiteral("Failed to resolve required OmniVoice symbols from runtime library.");
            m_lib.unload();
        }
        if (ok) {
            m_loadedPath = libPath;
            m_lastError.clear();
        }
        return ok;
    }

    bool isLoaded() const { return m_lib.isLoaded(); }
    QString errorString() const { return m_lastError.isEmpty() ? m_lib.errorString() : m_lastError; }

    ov_version_fn ov_version = nullptr;
    ov_last_error_fn ov_last_error = nullptr;
    ov_audio_free_fn ov_audio_free = nullptr;
    ov_init_default_params_fn ov_init_default_params = nullptr;
    ov_init_fn ov_init = nullptr;
    ov_free_fn ov_free = nullptr;
    ov_tts_default_params_fn ov_tts_default_params = nullptr;
    ov_synthesize_fn ov_synthesize = nullptr;
    ov_duration_sec_to_tokens_fn ov_duration_sec_to_tokens = nullptr;

private:
    OmnivoiceInterface() = default;
    QLibrary m_lib;
    QString m_loadedPath;
    QString m_lastError;
};

} // namespace LAStudio
