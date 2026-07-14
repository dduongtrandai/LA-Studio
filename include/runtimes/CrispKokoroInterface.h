#pragma once

#include <QLibrary>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QByteArray>
#include <QCoreApplication>
#include <stdbool.h>
#include <stdint.h>
#include <cstring>
#include "core/Logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "CrispCommon.h"

namespace LAStudio {

#ifdef Q_OS_WIN
typedef FILE* (*popen_fn)(const char*, const char*);
inline popen_fn original_popen = nullptr;

inline FILE* hooked_popen(const char* command, const char* mode) {
    if (command && strstr(command, "espeak-ng")) {
        std::string cmdStr(command);
        size_t langPos = cmdStr.find("-v ");
        if (langPos != std::string::npos) {
            langPos += 3;
            size_t langEnd = cmdStr.find(' ', langPos);
            if (langEnd != std::string::npos) {
                std::string lang = cmdStr.substr(langPos, langEnd - langPos);
                size_t textStart = cmdStr.find(" '", langEnd);
                if (textStart != std::string::npos) {
                    textStart += 2;
                    size_t textEnd = cmdStr.find_last_of('\'');
                    if (textEnd != std::string::npos && textEnd > textStart) {
                        std::string textRaw = cmdStr.substr(textStart, textEnd - textStart);
                        
                        // Unescape single quotes: replace '\'' with '
                        std::string text = textRaw;
                        size_t pos = 0;
                        while ((pos = text.find("'\\''", pos)) != std::string::npos) {
                            text.replace(pos, 4, "'");
                            pos += 1;
                        }
                        
                        // Escape quotes and backslashes for cmd.exe / MSVC runtime parser
                        std::string new_cmd = "espeak-ng -q --ipa=3 -v " + lang + " \"";
                        for (size_t i = 0; i < text.size(); ++i) {
                            char c = text[i];
                            if (c == '"') {
                                new_cmd += "\\\"";
                            } else if (c == '\\') {
                                size_t num_backslashes = 0;
                                while (i < text.size() && text[i] == '\\') {
                                    num_backslashes++;
                                    i++;
                                }
                                bool next_is_quote = (i < text.size() && text[i] == '"');
                                bool is_end = (i == text.size());
                                if (next_is_quote || is_end) {
                                    new_cmd.append(num_backslashes * 2, '\\');
                                } else {
                                    new_cmd.append(num_backslashes, '\\');
                                }
                                i--;
                            } else {
                                new_cmd += c;
                            }
                        }
                        new_cmd += "\"";
                        
                        if (original_popen) {
                            return original_popen(new_cmd.c_str(), mode);
                        }
                    }
                }
            }
        }
    }
    
    if (original_popen) {
        return original_popen(command, mode);
    }
    return nullptr;
}

inline void install_popen_hook(HMODULE hModule) {
    if (!hModule) return;
    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return;
    
    IMAGE_DATA_DIRECTORY importDirectory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDirectory.VirtualAddress == 0) return;
    
    PIMAGE_IMPORT_DESCRIPTOR importDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hModule + importDirectory.VirtualAddress);
    
    for (; importDescriptor->Name != 0; importDescriptor++) {
        PIMAGE_THUNK_DATA thunkLT = (PIMAGE_THUNK_DATA)((BYTE*)hModule + importDescriptor->OriginalFirstThunk);
        PIMAGE_THUNK_DATA thunkAT = (PIMAGE_THUNK_DATA)((BYTE*)hModule + importDescriptor->FirstThunk);
        
        for (; thunkLT->u1.AddressOfData != 0; thunkLT++, thunkAT++) {
            if (!(thunkLT->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                PIMAGE_IMPORT_BY_NAME importByName = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hModule + thunkLT->u1.AddressOfData);
                const char* funcName = (const char*)importByName->Name;
                
                if (strcmp(funcName, "_popen") == 0 || strcmp(funcName, "popen") == 0) {
                    original_popen = (popen_fn)thunkAT->u1.Function;
                    
                    DWORD oldProtect;
                    VirtualProtect(&thunkAT->u1.Function, sizeof(DWORD_PTR), PAGE_READWRITE, &oldProtect);
                    thunkAT->u1.Function = (DWORD_PTR)hooked_popen;
                    VirtualProtect(&thunkAT->u1.Function, sizeof(DWORD_PTR), oldProtect, &oldProtect);
                    
                    Logger::info("CrispKokoro", "Successfully hooked IAT entry for popen in crispasr.dll");
                    return;
                }
            }
        }
    }
}
#endif

class CrispKokoroInterface {
public:
    static CrispKokoroInterface& instance() {
        static CrispKokoroInterface inst;
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
        crispasr_session_open_explicit = nullptr;
        crispasr_session_close = nullptr;
        crispasr_session_set_voice = nullptr;
        crispasr_session_set_length_scale = nullptr;
        crispasr_session_synthesize = nullptr;
        crispasr_pcm_free = nullptr;
        crispasr_kokoro_resolve_model_for_lang = nullptr;
        crispasr_kokoro_resolve_fallback_voice = nullptr;
        m_errorString.clear();
    }

    bool load(const QString& libPath) {
        if (libPath.isEmpty()) return false;
        
        QString cleanLibPath = QDir::toNativeSeparators(QDir::cleanPath(libPath));

        if (m_lib.isLoaded()) {
            if (m_lib.fileName() == cleanLibPath) {
                configureKokoroPhonemizer(cleanLibPath);
                return true;
            }
            unload();
        }

        m_errorString.clear();
        Logger::info("CrispKokoro", "Loading library: " + cleanLibPath);

        QFileInfo fi(cleanLibPath);
        QString dir = QDir::toNativeSeparators(fi.absolutePath());
        QString ggmlDir = QDir::toNativeSeparators(QDir::cleanPath(fi.absolutePath() + "/../ggml/bin"));
        
        Logger::info("CrispKokoro", "DLL directory: " + dir);
        if (QDir(ggmlDir).exists()) {
             Logger::info("CrispKokoro", "Found ggml directory: " + ggmlDir);
        }

        QByteArray oldPath = qgetenv("PATH");
        const QStringList runtimeDirs = crispRuntimeDependencyDirs(cleanLibPath);
        crispPrependRuntimeDirsToPath(runtimeDirs);
        configureKokoroPhonemizer(cleanLibPath);

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
            Logger::error("CrispKokoro", "Failed to load library: " + m_errorString);
            qputenv("PATH", oldPath);
        #ifdef Q_OS_WIN
            crispReleasePreloadedRuntimeDlls(m_preloadedDlls);
        #endif
            return false;
        }

        #ifdef Q_OS_WIN
        HMODULE hModule = (HMODULE)GetModuleHandleW((LPCWSTR)cleanLibPath.utf16());
        if (hModule) {
            install_popen_hook(hModule);
        } else {
            Logger::error("CrispKokoro", "Failed to get HMODULE for popen hooking");
        }
        #endif

        Logger::info("CrispKokoro", "Library loaded, resolving symbols...");
        crispasr_session_open_with_params =
            (crispasr_session_open_with_params_fn)m_lib.resolve("crispasr_session_open_with_params");
        crispasr_session_open_explicit =
            (crispasr_session_open_explicit_fn)m_lib.resolve("crispasr_session_open_explicit");
        crispasr_session_close = (crispasr_session_close_fn)m_lib.resolve("crispasr_session_close");
        crispasr_session_set_voice = (crispasr_session_set_voice_fn)m_lib.resolve("crispasr_session_set_voice");
        crispasr_session_set_length_scale =
            (crispasr_session_set_length_scale_fn)m_lib.resolve("crispasr_session_set_length_scale");
        crispasr_session_synthesize =
            (crispasr_session_synthesize_fn)m_lib.resolve("crispasr_session_synthesize");
        crispasr_pcm_free = (crispasr_pcm_free_fn)m_lib.resolve("crispasr_pcm_free");
        crispasr_kokoro_resolve_model_for_lang =
            (crispasr_kokoro_resolve_model_for_lang_fn)m_lib.resolve("crispasr_kokoro_resolve_model_for_lang");
        crispasr_kokoro_resolve_fallback_voice =
            (crispasr_kokoro_resolve_fallback_voice_fn)m_lib.resolve("crispasr_kokoro_resolve_fallback_voice");

        ok = crispasr_session_close && crispasr_session_set_voice && crispasr_session_synthesize &&
             crispasr_pcm_free && (crispasr_session_open_with_params || crispasr_session_open_explicit);

        if (!ok) {
            m_errorString = QStringLiteral("Failed to resolve one or more required session symbols.");
            Logger::error("CrispKokoro", m_errorString);
#ifdef Q_OS_WIN
            crispUnloadLibraryAndDependencies(m_lib, m_preloadedDlls);
#else
            m_lib.unload();
#endif
        } else {
            Logger::info("CrispKokoro", "Session symbols resolved successfully.");
        }

        return ok;
    }

    bool isLoaded() const { return m_lib.isLoaded(); }
    QString errorString() const { return m_errorString.isEmpty() ? m_lib.errorString() : m_errorString; }
    bool hasKokoroPhonemizer() const { return m_kokoroPhonemizerAvailable; }
    QString kokoroPhonemizerPath() const { return m_kokoroPhonemizerPath; }
    QString kokoroPhonemizerErrorString() const {
        if (m_kokoroPhonemizerAvailable)
            return QString();
        if (!m_kokoroPhonemizerError.isEmpty())
            return m_kokoroPhonemizerError;
        return QStringLiteral("Kokoro requires espeak-ng, but espeak-ng was not found. Install espeak-ng or place espeak-ng.exe in the CrispASR runtime folder, its bin folder, or an espeak-ng/bin subfolder.");
    }

    typedef crispasr_session* (*crispasr_session_open_with_params_fn)(const char*, const char*, const crispasr_open_params_v1*);
    typedef crispasr_session* (*crispasr_session_open_explicit_fn)(const char*, const char*, int);
    typedef void (*crispasr_session_close_fn)(crispasr_session*);
    typedef int (*crispasr_session_set_voice_fn)(crispasr_session*, const char*, const char*);
    typedef int (*crispasr_session_set_length_scale_fn)(crispasr_session*, float);
    typedef float* (*crispasr_session_synthesize_fn)(crispasr_session*, const char*, int*);
    typedef void (*crispasr_pcm_free_fn)(float*);
    typedef int (*crispasr_kokoro_resolve_model_for_lang_fn)(const char*, const char*, char*, int);
    typedef int (*crispasr_kokoro_resolve_fallback_voice_fn)(const char*, const char*, char*, int, char*, int);

    crispasr_session_open_with_params_fn crispasr_session_open_with_params = nullptr;
    crispasr_session_open_explicit_fn crispasr_session_open_explicit = nullptr;
    crispasr_session_close_fn crispasr_session_close = nullptr;
    crispasr_session_set_voice_fn crispasr_session_set_voice = nullptr;
    crispasr_session_set_length_scale_fn crispasr_session_set_length_scale = nullptr;
    crispasr_session_synthesize_fn crispasr_session_synthesize = nullptr;
    crispasr_pcm_free_fn crispasr_pcm_free = nullptr;
    crispasr_kokoro_resolve_model_for_lang_fn crispasr_kokoro_resolve_model_for_lang = nullptr;
    crispasr_kokoro_resolve_fallback_voice_fn crispasr_kokoro_resolve_fallback_voice = nullptr;

    void* openKokoroSession(const QString& modelPath, int nThreads, bool useGpu, bool flashAttn) {
        if (!m_lib.isLoaded())
            return nullptr;

        QByteArray modelBytes = modelPath.toUtf8();
        if (crispasr_session_open_with_params) {
            crispasr_open_params_v1 params{};
            params.abi_version = 2;
            params.n_threads = nThreads > 0 ? nThreads : 4;
            params.use_gpu = useGpu ? 1 : 0;
            params.verbosity = 1;
            params.flash_attn = flashAttn ? 1 : 0;
            params.n_gpu_layers = -1;
            return crispasr_session_open_with_params(modelBytes.constData(), "kokoro", &params);
        }
        if (crispasr_session_open_explicit)
            return crispasr_session_open_explicit(modelBytes.constData(), "kokoro", nThreads > 0 ? nThreads : 4);
        return nullptr;
    }

    void closeKokoroSession(void* session) {
        if (session && crispasr_session_close)
            crispasr_session_close(static_cast<crispasr_session*>(session));
    }

    int loadVoicePack(void* session, const QString& voicePath) {
        if (!session || !crispasr_session_set_voice)
            return -1;
        QByteArray voiceBytes = voicePath.toUtf8();
        return crispasr_session_set_voice(static_cast<crispasr_session*>(session), voiceBytes.constData(), nullptr);
    }

    int setLengthScale(void* session, float scale) {
        if (!session || !crispasr_session_set_length_scale)
            return -2;
        return crispasr_session_set_length_scale(static_cast<crispasr_session*>(session), scale);
    }

    float* synthesize(void* session, const char* text, int* outSamples) {
        if (!session || !crispasr_session_synthesize)
            return nullptr;
        return crispasr_session_synthesize(static_cast<crispasr_session*>(session), text, outSamples);
    }

    void freePcm(float* pcm) {
        if (pcm && crispasr_pcm_free)
            crispasr_pcm_free(pcm);
    }

private:
    CrispKokoroInterface() = default;

    static bool containsPath(const QStringList& entries, const QString& path) {
        return entries.contains(QDir::toNativeSeparators(QDir::cleanPath(path)), Qt::CaseInsensitive);
    }

    static void prependPath(QStringList& entries, const QString& path) {
        const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(path));
        if (!cleanPath.isEmpty() && QDir(cleanPath).exists() && !containsPath(entries, cleanPath)) {
            entries.prepend(cleanPath);
        }
    }

    void configureKokoroPhonemizer(const QString& libPath) {
        const QFileInfo fi(libPath);
        const QDir runtimeRoot(QDir::cleanPath(fi.absolutePath() + QStringLiteral("/..")));
        const QString appDir = QCoreApplication::applicationDirPath();

        QStringList candidateDirs;
        candidateDirs
            << fi.absolutePath()
            << runtimeRoot.absolutePath()
            << runtimeRoot.absoluteFilePath(QStringLiteral("bin"))
            << runtimeRoot.absoluteFilePath(QStringLiteral("espeak-ng"))
            << runtimeRoot.absoluteFilePath(QStringLiteral("espeak-ng/bin"))
            << runtimeRoot.absoluteFilePath(QStringLiteral("tools/espeak-ng"))
            << runtimeRoot.absoluteFilePath(QStringLiteral("tools/espeak-ng/bin"))
            << runtimeRoot.absoluteFilePath(QStringLiteral("phonemizer"))
            << runtimeRoot.absoluteFilePath(QStringLiteral("phonemizer/bin"))
            << appDir
            << QDir(appDir).absoluteFilePath(QStringLiteral("espeak-ng"))
            << QDir(appDir).absoluteFilePath(QStringLiteral("espeak-ng/bin"))
            << QDir(appDir).absoluteFilePath(QStringLiteral("tools/espeak-ng"))
            << QDir(appDir).absoluteFilePath(QStringLiteral("tools/espeak-ng/bin"))
            << QStringLiteral("C:/Program Files/eSpeak NG")
            << QStringLiteral("C:/Program Files (x86)/eSpeak NG");

        const QByteArray oldPath = qgetenv("PATH");
        QStringList pathEntries = QString::fromLocal8Bit(oldPath).split(QDir::listSeparator(), Qt::SkipEmptyParts);
        candidateDirs.append(pathEntries);

#ifdef Q_OS_WIN
        const QString executableName = QStringLiteral("espeak-ng.exe");
#else
        const QString executableName = QStringLiteral("espeak-ng");
#endif

        QString foundExe;
        for (const QString& candidateDir : candidateDirs) {
            const QString exe = QDir(candidateDir).absoluteFilePath(executableName);
            if (QFileInfo::exists(exe)) {
                foundExe = QDir::toNativeSeparators(QDir::cleanPath(exe));
                prependPath(pathEntries, QFileInfo(foundExe).absolutePath());
                break;
            }
        }

        if (foundExe.isEmpty() && runtimeRoot.exists()) {
            QDirIterator it(runtimeRoot.absolutePath(),
                            QStringList{executableName},
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                foundExe = QDir::toNativeSeparators(QDir::cleanPath(it.next()));
                prependPath(pathEntries, QFileInfo(foundExe).absolutePath());
                break;
            }
        }

        if (!foundExe.isEmpty()) {
            qputenv("PATH", pathEntries.join(QDir::listSeparator()).toLocal8Bit());
            m_kokoroPhonemizerAvailable = configureEspeakDataPath(QFileInfo(foundExe).absolutePath());
            m_kokoroPhonemizerPath = foundExe;
            Logger::info("CrispKokoro", "Found Kokoro phonemizer: " + foundExe);
            if (!m_kokoroPhonemizerAvailable) {
                Logger::error("CrispKokoro", m_kokoroPhonemizerError);
            }
            return;
        }

        m_kokoroPhonemizerAvailable = false;
        m_kokoroPhonemizerPath.clear();
        m_kokoroPhonemizerError = QStringLiteral("Kokoro requires espeak-ng, but espeak-ng was not found. Install espeak-ng or place espeak-ng.exe in the CrispASR runtime folder, its bin folder, or an espeak-ng/bin subfolder.");
        Logger::error("CrispKokoro", kokoroPhonemizerErrorString());
    }

    bool configureEspeakDataPath(const QString& executableDir) {
        QStringList candidateDirs;
        const QDir exeDir(executableDir);
        candidateDirs
            << exeDir.absoluteFilePath(QStringLiteral("espeak-ng-data"))
            << exeDir.absoluteFilePath(QStringLiteral("../espeak-ng-data"))
            << exeDir.absoluteFilePath(QStringLiteral("../eSpeak NG/espeak-ng-data"));

        const QString existing = QString::fromLocal8Bit(qgetenv("ESPEAK_DATA_PATH"));
        if (!existing.isEmpty()) {
            candidateDirs.prepend(existing);
        }

        for (const QString& candidateDir : candidateDirs) {
            const QString cleanDir = QDir::toNativeSeparators(QDir::cleanPath(candidateDir));
            if (QDir(cleanDir).exists()) {
                qputenv("ESPEAK_DATA_PATH", cleanDir.toLocal8Bit());
                Logger::info("CrispKokoro", "Using eSpeak NG data path: " + cleanDir);
                m_kokoroPhonemizerError.clear();
                return true;
            }
        }

        m_kokoroPhonemizerError = QStringLiteral("Kokoro found espeak-ng.exe but could not locate espeak-ng-data next to it.");
        return false;
    }

    QLibrary m_lib;
    QString m_errorString;
#ifdef Q_OS_WIN
    QVector<HMODULE> m_preloadedDlls;
#endif
    bool m_kokoroPhonemizerAvailable = false;
    QString m_kokoroPhonemizerPath;
    QString m_kokoroPhonemizerError;
};

} // namespace LAStudio
