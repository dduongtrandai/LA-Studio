#pragma once

#include <QLibrary>
#include <QString>
#include <QVector>
#include <cstdint>
#include "CrispCommon.h"

namespace LAStudio {

struct crispasr_align_result;
struct crispasr_session_result;

class CrispAlignmentInterface {
public:
    struct Span { float start = 0; float end = 0; };
    struct Word { QString text; double start = 0; double end = 0; };

    ~CrispAlignmentInterface() { unload(); }
    bool load(const QString &libraryPath);
    void unload();
    bool isLoaded() const { return m_library.isLoaded(); }
    QString errorString() const { return m_error; }

    QVector<Span> vadSlices(const QString &modelPath, const QVector<float> &pcm,
                            float threshold, int minSpeechMs, int minSilenceMs,
                            int speechPadMs, float maxChunkSeconds, int threads) const;
    QVector<Word> align(const QString &modelPath, const QString &transcript,
                        const QVector<float> &pcm, int64_t offsetCs, int threads) const;
    QString transcribe(const QString &modelPath, const QString &backend, const QString &language,
                       const QVector<float> &pcm, int threads, bool useGpu,
                       QString *error = nullptr) const;

private:
    using vad_slices_fn = int (*)(const char*, const float*, int, int, float, int, int, int, float, int, float**);
    using vad_free_fn = void (*)(float*);
    using align_words_fn = crispasr_align_result* (*)(const char*, const char*, const float*, int32_t, int64_t, int32_t);
    using result_count_fn = int (*)(crispasr_align_result*);
    using result_text_fn = const char* (*)(crispasr_align_result*, int);
    using result_time_fn = int64_t (*)(crispasr_align_result*, int);
    using result_free_fn = void (*)(crispasr_align_result*);
    using session_open_fn = crispasr_session* (*)(const char*, const char*, const crispasr_open_params_v1*);
    using session_close_fn = void (*)(crispasr_session*);
    using session_transcribe_fn = crispasr_session_result* (*)(crispasr_session*, const float*, int, const char*);
    using session_count_fn = int (*)(crispasr_session_result*);
    using session_text_fn = const char* (*)(crispasr_session_result*, int);
    using session_free_fn = void (*)(crispasr_session_result*);

    QLibrary m_library;
#ifdef Q_OS_WIN
    QVector<HMODULE> m_preloadedDlls;
#endif
    QString m_error;
    vad_slices_fn m_vadSlices = nullptr;
    vad_free_fn m_vadFree = nullptr;
    align_words_fn m_alignWords = nullptr;
    result_count_fn m_resultCount = nullptr;
    result_text_fn m_resultText = nullptr;
    result_time_fn m_resultT0 = nullptr;
    result_time_fn m_resultT1 = nullptr;
    result_free_fn m_resultFree = nullptr;
    session_open_fn m_sessionOpen = nullptr;
    session_close_fn m_sessionClose = nullptr;
    session_transcribe_fn m_sessionTranscribe = nullptr;
    session_count_fn m_sessionCount = nullptr;
    session_text_fn m_sessionText = nullptr;
    session_free_fn m_sessionFree = nullptr;
};

} // namespace LAStudio
