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

class CrispQwen3TtsInterface {
public:
    static CrispQwen3TtsInterface& instance() {
        static CrispQwen3TtsInterface inst;
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
        crispasr_session_set_codec_path = nullptr;
        crispasr_session_set_voice = nullptr;
        crispasr_session_set_speaker_name = nullptr;
        crispasr_session_n_speakers = nullptr;
        crispasr_session_get_speaker_name = nullptr;
        crispasr_session_set_instruct = nullptr;
        crispasr_session_is_custom_voice = nullptr;
        crispasr_session_is_voice_design = nullptr;
        crispasr_session_set_float = nullptr;
        crispasr_session_set_int = nullptr;
        crispasr_session_set_temperature = nullptr;
        crispasr_session_synthesize = nullptr;
        crispasr_pcm_free = nullptr;
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
        Logger::info("CrispQwen3Tts", "Loading library: " + cleanLibPath);

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
            Logger::error("CrispQwen3Tts", "Failed to load library: " + m_errorString);
#ifdef Q_OS_WIN
            crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
#endif
            qputenv("PATH", oldPath);
            return false;
        }

        Logger::info("CrispQwen3Tts", "Library loaded, resolving symbols...");
        
        crispasr_session_open_with_params = (crispasr_session_open_with_params_fn)m_lib.resolve("crispasr_session_open_with_params");
        crispasr_session_close = (crispasr_session_close_fn)m_lib.resolve("crispasr_session_close");
        crispasr_session_set_codec_path = (crispasr_session_set_codec_path_fn)m_lib.resolve("crispasr_session_set_codec_path");
        crispasr_session_set_voice = (crispasr_session_set_voice_fn)m_lib.resolve("crispasr_session_set_voice");
        crispasr_session_set_speaker_name = (crispasr_session_set_speaker_name_fn)m_lib.resolve("crispasr_session_set_speaker_name");
        crispasr_session_n_speakers = (crispasr_session_n_speakers_fn)m_lib.resolve("crispasr_session_n_speakers");
        crispasr_session_get_speaker_name = (crispasr_session_get_speaker_name_fn)m_lib.resolve("crispasr_session_get_speaker_name");
        crispasr_session_set_instruct = (crispasr_session_set_instruct_fn)m_lib.resolve("crispasr_session_set_instruct");
        crispasr_session_is_custom_voice = (crispasr_session_is_custom_voice_fn)m_lib.resolve("crispasr_session_is_custom_voice");
        crispasr_session_is_voice_design = (crispasr_session_is_voice_design_fn)m_lib.resolve("crispasr_session_is_voice_design");
        crispasr_session_set_float = (crispasr_session_set_float_fn)m_lib.resolve("crispasr_session_set_float");
        crispasr_session_set_int = (crispasr_session_set_int_fn)m_lib.resolve("crispasr_session_set_int");
        crispasr_session_set_temperature = (crispasr_session_set_temperature_fn)m_lib.resolve("crispasr_session_set_temperature");
        crispasr_session_synthesize = (crispasr_session_synthesize_fn)m_lib.resolve("crispasr_session_synthesize");
        crispasr_pcm_free = (crispasr_pcm_free_fn)m_lib.resolve("crispasr_pcm_free");

        ok = crispasr_session_open_with_params && 
             crispasr_session_close && 
             crispasr_session_set_codec_path && 
             crispasr_session_synthesize && 
             crispasr_pcm_free;

        if (!ok) {
            m_errorString = QStringLiteral("Failed to resolve one or more required session symbols for Qwen3-TTS.");
            Logger::error("CrispQwen3Tts", m_errorString);
#ifdef Q_OS_WIN
            crispUnloadLibraryAndDependencies(m_lib, m_preloadedDlls);
#else
            m_lib.unload();
#endif
        } else {
            Logger::info("CrispQwen3Tts", "Session symbols resolved successfully.");
        }

        return ok;
    }

    bool isLoaded() const { return m_lib.isLoaded(); }
    QString errorString() const { return m_errorString.isEmpty() ? m_lib.errorString() : m_errorString; }

    typedef crispasr_session* (*crispasr_session_open_with_params_fn)(const char* model_path, const char* backend, const crispasr_open_params_v1* params);
    typedef void (*crispasr_session_close_fn)(crispasr_session* session);
    typedef int (*crispasr_session_set_codec_path_fn)(crispasr_session* session, const char* codec_path);
    typedef int (*crispasr_session_set_voice_fn)(crispasr_session* session, const char* voice_path, const char* ref_text);
    typedef int (*crispasr_session_set_speaker_name_fn)(crispasr_session* session, const char* speaker_name);
    typedef int (*crispasr_session_n_speakers_fn)(crispasr_session* session);
    typedef const char* (*crispasr_session_get_speaker_name_fn)(crispasr_session* session, int index);
    typedef int (*crispasr_session_set_instruct_fn)(crispasr_session* session, const char* instruct);
    typedef int (*crispasr_session_is_custom_voice_fn)(crispasr_session* session);
    typedef int (*crispasr_session_is_voice_design_fn)(crispasr_session* session);
    typedef int (*crispasr_session_set_float_fn)(crispasr_session* session, const char* name, float value);
    typedef int (*crispasr_session_set_int_fn)(crispasr_session* session, const char* name, int value);
    typedef int (*crispasr_session_set_temperature_fn)(crispasr_session* session, float temperature, uint64_t seed);
    typedef float* (*crispasr_session_synthesize_fn)(crispasr_session* session, const char* text, int* out_samples);
    typedef void (*crispasr_pcm_free_fn)(float* pcm);

    crispasr_session_open_with_params_fn crispasr_session_open_with_params = nullptr;
    crispasr_session_close_fn crispasr_session_close = nullptr;
    crispasr_session_set_codec_path_fn crispasr_session_set_codec_path = nullptr;
    crispasr_session_set_voice_fn crispasr_session_set_voice = nullptr;
    crispasr_session_set_speaker_name_fn crispasr_session_set_speaker_name = nullptr;
    crispasr_session_n_speakers_fn crispasr_session_n_speakers = nullptr;
    crispasr_session_get_speaker_name_fn crispasr_session_get_speaker_name = nullptr;
    crispasr_session_set_instruct_fn crispasr_session_set_instruct = nullptr;
    crispasr_session_is_custom_voice_fn crispasr_session_is_custom_voice = nullptr;
    crispasr_session_is_voice_design_fn crispasr_session_is_voice_design = nullptr;
    crispasr_session_set_float_fn crispasr_session_set_float = nullptr;
    crispasr_session_set_int_fn crispasr_session_set_int = nullptr;
    crispasr_session_set_temperature_fn crispasr_session_set_temperature = nullptr;
    crispasr_session_synthesize_fn crispasr_session_synthesize = nullptr;
    crispasr_pcm_free_fn crispasr_pcm_free = nullptr;

private:
    CrispQwen3TtsInterface() = default;
    QLibrary m_lib;
    QString m_errorString;
#ifdef Q_OS_WIN
    QVector<HMODULE> m_preloadedDlls;
#endif
};

} // namespace LAStudio
