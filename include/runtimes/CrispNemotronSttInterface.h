#pragma once

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QString>
#include <QStringList>
#include <QVector>

#include "CrispCommon.h"
#include "core/Logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LAStudio {

struct crispasr_session_result;

// C ABI wrapper for the Nemotron backend introduced by CrispASR v0.8.0.
class CrispNemotronSttInterface {
public:
    static CrispNemotronSttInterface &instance()
    {
        static CrispNemotronSttInterface inst;
        return inst;
    }

    void unload()
    {
#ifdef Q_OS_WIN
        crispUnloadLibraryAndDependencies(m_lib, m_preloadedDlls);
#else
        if (m_lib.isLoaded()) m_lib.unload();
#endif
        crispasr_session_open_with_params = nullptr;
        crispasr_session_close = nullptr;
        crispasr_session_transcribe_lang = nullptr;
        crispasr_session_result_n_segments = nullptr;
        crispasr_session_result_segment_text = nullptr;
        crispasr_session_result_segment_t0 = nullptr;
        crispasr_session_result_segment_t1 = nullptr;
        crispasr_session_result_free = nullptr;
        crispasr_session_set_beam_size = nullptr;
        m_errorString.clear();
    }

    bool load(const QString &libPath)
    {
        if (libPath.isEmpty()) return false;
        const QString cleanLibPath = QDir::toNativeSeparators(QDir::cleanPath(libPath));
        if (m_lib.isLoaded()) {
            if (m_lib.fileName() == cleanLibPath) return true;
            unload();
        }

        m_errorString.clear();
        const QByteArray oldPath = qgetenv("PATH");
        const QStringList runtimeDirs = crispRuntimeDependencyDirs(cleanLibPath);
        crispPrependRuntimeDirsToPath(runtimeDirs);
#ifdef Q_OS_WIN
        const QFileInfo fileInfo(cleanLibPath);
        crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
        m_preloadedDlls = crispPreloadRuntimeDlls(cleanLibPath, runtimeDirs);
        SetDllDirectoryW(reinterpret_cast<LPCWSTR>(fileInfo.absolutePath().utf16()));
#endif
        m_lib.setFileName(cleanLibPath);
        m_lib.setLoadHints(QLibrary::ExportExternalSymbolsHint);
        const bool loaded = m_lib.load();
#ifdef Q_OS_WIN
        SetDllDirectoryW(nullptr);
#endif
        if (!loaded) {
            m_errorString = m_lib.errorString();
#ifdef Q_OS_WIN
            crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
#endif
            qputenv("PATH", oldPath);
            return false;
        }

        crispasr_session_open_with_params = reinterpret_cast<crispasr_session_open_with_params_fn>(m_lib.resolve("crispasr_session_open_with_params"));
        crispasr_session_close = reinterpret_cast<crispasr_session_close_fn>(m_lib.resolve("crispasr_session_close"));
        crispasr_session_transcribe_lang = reinterpret_cast<crispasr_session_transcribe_lang_fn>(m_lib.resolve("crispasr_session_transcribe_lang"));
        crispasr_session_result_n_segments = reinterpret_cast<crispasr_session_result_n_segments_fn>(m_lib.resolve("crispasr_session_result_n_segments"));
        crispasr_session_result_segment_text = reinterpret_cast<crispasr_session_result_segment_text_fn>(m_lib.resolve("crispasr_session_result_segment_text"));
        crispasr_session_result_segment_t0 = reinterpret_cast<crispasr_session_result_segment_t0_fn>(m_lib.resolve("crispasr_session_result_segment_t0"));
        crispasr_session_result_segment_t1 = reinterpret_cast<crispasr_session_result_segment_t1_fn>(m_lib.resolve("crispasr_session_result_segment_t1"));
        crispasr_session_result_free = reinterpret_cast<crispasr_session_result_free_fn>(m_lib.resolve("crispasr_session_result_free"));
        crispasr_session_set_beam_size = reinterpret_cast<crispasr_session_set_beam_size_fn>(m_lib.resolve("crispasr_session_set_beam_size"));

        const bool ok = crispasr_session_open_with_params && crispasr_session_close &&
                        crispasr_session_transcribe_lang && crispasr_session_result_n_segments &&
                        crispasr_session_result_segment_text && crispasr_session_result_segment_t0 &&
                        crispasr_session_result_segment_t1 && crispasr_session_result_free;
        if (!ok) {
            m_errorString = QStringLiteral("CrispASR v0.8.0 or later is required for Nemotron STT.");
#ifdef Q_OS_WIN
            crispUnloadLibraryAndDependencies(m_lib, m_preloadedDlls);
#else
            m_lib.unload();
#endif
        }
        return ok;
    }

    bool isLoaded() const { return m_lib.isLoaded(); }
    QString errorString() const { return m_errorString.isEmpty() ? m_lib.errorString() : m_errorString; }

    using crispasr_session_open_with_params_fn = crispasr_session *(*)(const char *, const char *, const crispasr_open_params_v1 *);
    using crispasr_session_close_fn = void (*)(crispasr_session *);
    using crispasr_session_transcribe_lang_fn = crispasr_session_result *(*)(crispasr_session *, const float *, int, const char *);
    using crispasr_session_result_n_segments_fn = int (*)(crispasr_session_result *);
    using crispasr_session_result_segment_text_fn = const char *(*)(crispasr_session_result *, int);
    using crispasr_session_result_segment_t0_fn = int64_t (*)(crispasr_session_result *, int);
    using crispasr_session_result_segment_t1_fn = int64_t (*)(crispasr_session_result *, int);
    using crispasr_session_result_free_fn = void (*)(crispasr_session_result *);
    using crispasr_session_set_beam_size_fn = int (*)(crispasr_session *, int);

    crispasr_session_open_with_params_fn crispasr_session_open_with_params = nullptr;
    crispasr_session_close_fn crispasr_session_close = nullptr;
    crispasr_session_transcribe_lang_fn crispasr_session_transcribe_lang = nullptr;
    crispasr_session_result_n_segments_fn crispasr_session_result_n_segments = nullptr;
    crispasr_session_result_segment_text_fn crispasr_session_result_segment_text = nullptr;
    crispasr_session_result_segment_t0_fn crispasr_session_result_segment_t0 = nullptr;
    crispasr_session_result_segment_t1_fn crispasr_session_result_segment_t1 = nullptr;
    crispasr_session_result_free_fn crispasr_session_result_free = nullptr;
    crispasr_session_set_beam_size_fn crispasr_session_set_beam_size = nullptr;

private:
    CrispNemotronSttInterface() = default;
    QLibrary m_lib;
    QString m_errorString;
#ifdef Q_OS_WIN
    QVector<HMODULE> m_preloadedDlls;
#endif
};

} // namespace LAStudio
