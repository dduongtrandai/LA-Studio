#pragma once

#include <QLibrary>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <stdint.h>
#include <stdbool.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Re-declare needed types from whisper.h to avoid header dependency
struct whisper_context;
struct whisper_state;
typedef int32_t whisper_token;

enum whisper_sampling_strategy {
    WHISPER_SAMPLING_GREEDY      = 0,
    WHISPER_SAMPLING_BEAM_SEARCH = 1,
};

enum whisper_alignment_heads_preset {
    WHISPER_AHEADS_NONE,
    WHISPER_AHEADS_N_TOP_MOST,
    WHISPER_AHEADS_CUSTOM,
    WHISPER_AHEADS_TINY_EN,
    WHISPER_AHEADS_TINY,
    WHISPER_AHEADS_BASE_EN,
    WHISPER_AHEADS_BASE,
    WHISPER_AHEADS_SMALL_EN,
    WHISPER_AHEADS_SMALL,
    WHISPER_AHEADS_MEDIUM_EN,
    WHISPER_AHEADS_MEDIUM,
    WHISPER_AHEADS_LARGE_V1,
    WHISPER_AHEADS_LARGE_V2,
    WHISPER_AHEADS_LARGE_V3,
    WHISPER_AHEADS_LARGE_V3_TURBO,
};

typedef struct whisper_ahead {
    int n_text_layer;
    int n_head;
} whisper_ahead;

typedef struct whisper_aheads {
    size_t n_heads;
    const whisper_ahead * heads;
} whisper_aheads;

struct whisper_context_params {
    bool  use_gpu;
    bool  flash_attn;
    int   gpu_device;

    bool dtw_token_timestamps;
    enum whisper_alignment_heads_preset dtw_aheads_preset;

    int dtw_n_top;
    struct whisper_aheads dtw_aheads;

    size_t dtw_mem_size;
};

typedef struct whisper_token_data {
    whisper_token id;
    whisper_token tid;

    float p;
    float plog;
    float pt;
    float ptsum;

    int64_t t0;
    int64_t t1;

    int64_t t_dtw;

    float vlen;
} whisper_token_data;

typedef struct whisper_grammar_element {
    int type;
    uint32_t value;
} whisper_grammar_element;

typedef struct whisper_vad_params {
    float threshold;
    int   min_speech_duration_ms;
    int   min_silence_duration_ms;
    float max_speech_duration_s;
    int   speech_pad_ms;
    float samples_overlap;
} whisper_vad_params;

typedef void (*whisper_new_segment_callback)(struct whisper_context * ctx, struct whisper_state * state, int n_new, void * user_data);
typedef void (*whisper_progress_callback)(struct whisper_context * ctx, struct whisper_state * state, int progress, void * user_data);
typedef bool (*whisper_encoder_begin_callback)(struct whisper_context * ctx, struct whisper_state * state, void * user_data);
typedef bool (*ggml_abort_callback)(void * data);
typedef void (*whisper_logits_filter_callback)(
        struct whisper_context * ctx,
          struct whisper_state * state,
      const whisper_token_data * tokens,
                           int   n_tokens,
                         float * logits,
                          void * user_data);

struct whisper_full_params {
    enum whisper_sampling_strategy strategy;

    int n_threads;
    int n_max_text_ctx;
    int offset_ms;
    int duration_ms;

    bool translate;
    bool no_context;
    bool no_timestamps;
    bool single_segment;
    bool print_special;
    bool print_progress;
    bool print_realtime;
    bool print_timestamps;

    bool  token_timestamps;
    float thold_pt;
    float thold_ptsum;
    int   max_len;
    bool  split_on_word;
    int   max_tokens;

    bool debug_mode;
    int  audio_ctx;

    bool tdrz_enable;

    const char * suppress_regex;

    const char * initial_prompt;
    bool carry_initial_prompt;
    const whisper_token * prompt_tokens;
    int prompt_n_tokens;

    const char * language;
    bool detect_language;

    bool suppress_blank;
    bool suppress_nst;

    float temperature;
    float max_initial_ts;
    float length_penalty;

    float temperature_inc;
    float entropy_thold;
    float logprob_thold;
    float no_speech_thold;

    struct {
        int best_of;
    } greedy;

    struct {
        int beam_size;
        float patience;
    } beam_search;

    whisper_new_segment_callback new_segment_callback;
    void * new_segment_callback_user_data;

    whisper_progress_callback progress_callback;
    void * progress_callback_user_data;

    whisper_encoder_begin_callback encoder_begin_callback;
    void * encoder_begin_callback_user_data;

    ggml_abort_callback abort_callback;
    void * abort_callback_user_data;

    whisper_logits_filter_callback logits_filter_callback;
    void * logits_filter_callback_user_data;

    const whisper_grammar_element ** grammar_rules;
    size_t                           n_grammar_rules;
    size_t                           i_start_rule;
    float                            grammar_penalty;

    bool         vad;
    const char * vad_model_path;

    whisper_vad_params vad_params;
};

namespace LAStudio {

/**
 * @brief WhisperInterface manages dynamic loading of whisper.cpp functions from a shared library (DLL/SO/dylib).
 * This follows the "Pluggable Runtime" pattern used by LM Studio.
 */
class WhisperInterface {
public:
    static WhisperInterface& instance() {
        static WhisperInterface inst;
        return inst;
    }

    void unload() {
        // Keep the DLL loaded to prevent OpenMP/GGML runtime crashes on unload/reload.
        // The model memory itself is freed via free_context.
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

        if (!ok) {
            return false;
        }

        // Resolve symbols
        init_from_file = (whisper_init_from_file_with_params_fn)m_lib.resolve("whisper_init_from_file_with_params");
        free_context = (whisper_free_fn)m_lib.resolve("whisper_free");
        context_default_params = (whisper_context_default_params_fn)m_lib.resolve("whisper_context_default_params");
        full_default_params = (whisper_full_default_params_fn)m_lib.resolve("whisper_full_default_params");
        full_run = (whisper_full_fn)m_lib.resolve("whisper_full");
        full_n_segments = (whisper_full_n_segments_fn)m_lib.resolve("whisper_full_n_segments");
        full_get_segment_text = (whisper_full_get_segment_text_fn)m_lib.resolve("whisper_full_get_segment_text");
        full_get_segment_t0 = (whisper_full_get_segment_t0_fn)m_lib.resolve("whisper_full_get_segment_t0");
        full_get_segment_t1 = (whisper_full_get_segment_t1_fn)m_lib.resolve("whisper_full_get_segment_t1");
        print_system_info = (whisper_print_system_info_fn)m_lib.resolve("whisper_print_system_info");

        ok = init_from_file && free_context && context_default_params &&
                  full_default_params && full_run && full_n_segments &&
                  full_get_segment_text && full_get_segment_t0 && full_get_segment_t1;

        if (!ok) {
            m_lib.unload();
        }
        return ok;
    }

    bool isLoaded() const { return m_lib.isLoaded(); }
    QString errorString() const { return m_lib.errorString(); }

    // Function pointers
    typedef struct whisper_context* (*whisper_init_from_file_with_params_fn)(const char*, struct whisper_context_params);
    typedef void (*whisper_free_fn)(struct whisper_context*);
    typedef struct whisper_context_params (*whisper_context_default_params_fn)(void);
    typedef struct whisper_full_params (*whisper_full_default_params_fn)(enum whisper_sampling_strategy);
    typedef int (*whisper_full_fn)(struct whisper_context*, struct whisper_full_params, const float*, int);
    typedef int (*whisper_full_n_segments_fn)(struct whisper_context*);
    typedef const char* (*whisper_full_get_segment_text_fn)(struct whisper_context*, int);
    typedef int64_t (*whisper_full_get_segment_t0_fn)(struct whisper_context*, int);
    typedef int64_t (*whisper_full_get_segment_t1_fn)(struct whisper_context*, int);
    typedef const char* (*whisper_print_system_info_fn)(void);

    whisper_init_from_file_with_params_fn init_from_file = nullptr;
    whisper_free_fn free_context = nullptr;
    whisper_context_default_params_fn context_default_params = nullptr;
    whisper_full_default_params_fn full_default_params = nullptr;
    whisper_full_fn full_run = nullptr;
    whisper_full_n_segments_fn full_n_segments = nullptr;
    whisper_full_get_segment_text_fn full_get_segment_text = nullptr;
    whisper_full_get_segment_t0_fn full_get_segment_t0 = nullptr;
    whisper_full_get_segment_t1_fn full_get_segment_t1 = nullptr;
    whisper_print_system_info_fn print_system_info = nullptr;

private:
    WhisperInterface() = default;
    QLibrary m_lib;
};

} // namespace LAStudio

