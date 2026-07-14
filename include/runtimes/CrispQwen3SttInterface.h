#pragma once

#include <QLibrary>
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>
#include <stdbool.h>
#include <stdint.h>
#include "core/Logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "CrispCommon.h"

namespace LAStudio {

struct crispasr_session_result;

class CrispQwen3SttInterface {
public:
    static CrispQwen3SttInterface& instance() {
        static CrispQwen3SttInterface inst;
        return inst;
    }

    void unload() {
#ifdef Q_OS_WIN
        crispUnloadLibraryAndDependencies(m_lib, m_preloadedDlls);
#else
        if (m_lib.isLoaded()) {
            m_lib.unload();
        }
#endif
        crispasr_session_open_with_params = nullptr;
        crispasr_session_close = nullptr;
        crispasr_session_transcribe_lang = nullptr;
        crispasr_session_result_n_segments = nullptr;
        crispasr_session_result_segment_text = nullptr;
        crispasr_session_result_segment_t0 = nullptr;
        crispasr_session_result_segment_t1 = nullptr;
        crispasr_session_result_free = nullptr;
        crispasr_session_set_temperature = nullptr;
        crispasr_session_set_max_new_tokens = nullptr;
        crispasr_session_set_beam_size = nullptr;
        crispasr_session_set_top_p = nullptr;
        crispasr_session_set_ask = nullptr;
        m_errorString.clear();
    }

    bool load(const QString& libPath) {
        if (libPath.isEmpty()) return false;
        
        QString cleanLibPath = QDir::toNativeSeparators(QDir::cleanPath(libPath));

        if (m_lib.isLoaded()) {
            if (m_lib.fileName() == cleanLibPath) {
                return true;
            }
            unload();
        }

        m_errorString.clear();
        Logger::info("CrispQwen3Stt", "Loading library: " + cleanLibPath);

        QFileInfo fi(cleanLibPath);
        QString dir = QDir::toNativeSeparators(fi.absolutePath());
        QByteArray oldPath = qgetenv("PATH");
        const QStringList runtimeDirs = crispRuntimeDependencyDirs(cleanLibPath);
        crispPrependRuntimeDirsToPath(runtimeDirs);

#ifdef Q_OS_WIN
        crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
        m_preloadedDlls = crispPreloadRuntimeDlls(cleanLibPath, runtimeDirs);
        SetDllDirectoryW((LPCWSTR)dir.utf16());
#endif

        m_lib.setFileName(cleanLibPath);
        m_lib.setLoadHints(QLibrary::ExportExternalSymbolsHint);
        bool ok = m_lib.load();

#ifdef Q_OS_WIN
        SetDllDirectoryW(NULL);
#endif

        if (!ok) {
            m_errorString = m_lib.errorString();
            Logger::error("CrispQwen3Stt", "Failed to load library: " + m_errorString);
#ifdef Q_OS_WIN
            crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
#endif
            qputenv("PATH", oldPath);
            return false;
        }

        Logger::info("CrispQwen3Stt", "Library loaded, resolving symbols...");
        
        crispasr_session_open_with_params = (crispasr_session_open_with_params_fn)m_lib.resolve("crispasr_session_open_with_params");
        crispasr_session_close = (crispasr_session_close_fn)m_lib.resolve("crispasr_session_close");
        crispasr_session_transcribe_lang = (crispasr_session_transcribe_lang_fn)m_lib.resolve("crispasr_session_transcribe_lang");
        crispasr_session_result_n_segments = (crispasr_session_result_n_segments_fn)m_lib.resolve("crispasr_session_result_n_segments");
        crispasr_session_result_segment_text = (crispasr_session_result_segment_text_fn)m_lib.resolve("crispasr_session_result_segment_text");
        crispasr_session_result_segment_t0 = (crispasr_session_result_segment_t0_fn)m_lib.resolve("crispasr_session_result_segment_t0");
        crispasr_session_result_segment_t1 = (crispasr_session_result_segment_t1_fn)m_lib.resolve("crispasr_session_result_segment_t1");
        crispasr_session_result_free = (crispasr_session_result_free_fn)m_lib.resolve("crispasr_session_result_free");
        
        crispasr_session_set_temperature = (crispasr_session_set_temperature_fn)m_lib.resolve("crispasr_session_set_temperature");
        crispasr_session_set_max_new_tokens = (crispasr_session_set_max_new_tokens_fn)m_lib.resolve("crispasr_session_set_max_new_tokens");
        crispasr_session_set_beam_size = (crispasr_session_set_beam_size_fn)m_lib.resolve("crispasr_session_set_beam_size");
        crispasr_session_set_top_p = (crispasr_session_set_top_p_fn)m_lib.resolve("crispasr_session_set_top_p");
        crispasr_session_set_ask = (crispasr_session_set_ask_fn)m_lib.resolve("crispasr_session_set_ask");

        ok = crispasr_session_open_with_params && 
             crispasr_session_close && 
             crispasr_session_transcribe_lang && 
             crispasr_session_result_n_segments &&
             crispasr_session_result_segment_text &&
             crispasr_session_result_segment_t0 &&
             crispasr_session_result_segment_t1 &&
             crispasr_session_result_free;

        if (!ok) {
            m_errorString = QStringLiteral("Failed to resolve one or more required session symbols for CrispQwen3-STT.");
            Logger::error("CrispQwen3Stt", m_errorString);
#ifdef Q_OS_WIN
            crispUnloadLibraryAndDependencies(m_lib, m_preloadedDlls);
#else
            m_lib.unload();
#endif
        } else {
            Logger::info("CrispQwen3Stt", "Session symbols resolved successfully.");
        }

        return ok;
    }

    bool isLoaded() const { return m_lib.isLoaded(); }
    QString errorString() const { return m_errorString.isEmpty() ? m_lib.errorString() : m_errorString; }

    typedef crispasr_session* (*crispasr_session_open_with_params_fn)(const char* model_path, const char* backend, const crispasr_open_params_v1* params);
    typedef void (*crispasr_session_close_fn)(crispasr_session* session);
    typedef crispasr_session_result* (*crispasr_session_transcribe_lang_fn)(crispasr_session* session, const float* pcm, int n_samples, const char* language);
    typedef int (*crispasr_session_result_n_segments_fn)(crispasr_session_result* result);
    typedef const char* (*crispasr_session_result_segment_text_fn)(crispasr_session_result* result, int index);
    typedef int64_t (*crispasr_session_result_segment_t0_fn)(crispasr_session_result* result, int index);
    typedef int64_t (*crispasr_session_result_segment_t1_fn)(crispasr_session_result* result, int index);
    typedef void (*crispasr_session_result_free_fn)(crispasr_session_result* result);
    
    typedef int (*crispasr_session_set_temperature_fn)(crispasr_session* session, float temperature, uint64_t seed);
    typedef int (*crispasr_session_set_max_new_tokens_fn)(crispasr_session* session, int n);
    typedef int (*crispasr_session_set_beam_size_fn)(crispasr_session* session, int n);
    typedef int (*crispasr_session_set_top_p_fn)(crispasr_session* session, float top_p);
    typedef int (*crispasr_session_set_ask_fn)(crispasr_session* session, const char* prompt);

    crispasr_session_open_with_params_fn crispasr_session_open_with_params = nullptr;
    crispasr_session_close_fn crispasr_session_close = nullptr;
    crispasr_session_transcribe_lang_fn crispasr_session_transcribe_lang = nullptr;
    crispasr_session_result_n_segments_fn crispasr_session_result_n_segments = nullptr;
    crispasr_session_result_segment_text_fn crispasr_session_result_segment_text = nullptr;
    crispasr_session_result_segment_t0_fn crispasr_session_result_segment_t0 = nullptr;
    crispasr_session_result_segment_t1_fn crispasr_session_result_segment_t1 = nullptr;
    crispasr_session_result_free_fn crispasr_session_result_free = nullptr;
    
    crispasr_session_set_temperature_fn crispasr_session_set_temperature = nullptr;
    crispasr_session_set_max_new_tokens_fn crispasr_session_set_max_new_tokens = nullptr;
    crispasr_session_set_beam_size_fn crispasr_session_set_beam_size = nullptr;
    crispasr_session_set_top_p_fn crispasr_session_set_top_p = nullptr;
    crispasr_session_set_ask_fn crispasr_session_set_ask = nullptr;

private:
    CrispQwen3SttInterface() = default;
    QLibrary m_lib;
    QString m_errorString;
#ifdef Q_OS_WIN
    QVector<HMODULE> m_preloadedDlls;
#endif
};

} // namespace LAStudio
