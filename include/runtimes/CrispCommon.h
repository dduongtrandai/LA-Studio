#pragma once

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLibrary>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace LAStudio {

struct crispasr_session;

struct crispasr_open_params_v1 {
    int abi_version;
    int n_threads;
    int use_gpu;
    int verbosity;
    int flash_attn;
    int n_gpu_layers;
    int reserved[6];
};

inline bool crispContainsPath(const QStringList& entries, const QString& path)
{
    const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(path));
    return entries.contains(cleanPath, Qt::CaseInsensitive);
}

inline void crispPrependPath(QStringList& entries, const QString& path)
{
    const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (!cleanPath.isEmpty() && QDir(cleanPath).exists() && !crispContainsPath(entries, cleanPath)) {
        entries.prepend(cleanPath);
    }
}

inline QStringList crispRuntimeDependencyDirs(const QString& libPath)
{
    const QFileInfo libInfo(libPath);
    const QDir runtimeRoot(QDir::cleanPath(libInfo.absolutePath() + QStringLiteral("/..")));

    QStringList dirs;
    auto addDir = [&dirs](const QString& path) {
        const QString cleanPath = QDir::toNativeSeparators(QDir::cleanPath(path));
        if (!cleanPath.isEmpty() && QDir(cleanPath).exists() && !crispContainsPath(dirs, cleanPath)) {
            dirs.append(cleanPath);
        }
    };

    addDir(libInfo.absolutePath());
    addDir(runtimeRoot.absoluteFilePath(QStringLiteral("bin")));
    addDir(runtimeRoot.absoluteFilePath(QStringLiteral("ggml/bin")));
    addDir(runtimeRoot.absoluteFilePath(QStringLiteral("espeak-ng")));
    addDir(runtimeRoot.absoluteFilePath(QStringLiteral("espeak-ng/bin")));
    addDir(runtimeRoot.absoluteFilePath(QStringLiteral("espeak-ng/eSpeak NG")));
    addDir(runtimeRoot.absoluteFilePath(QStringLiteral("espeak-ng/eSpeak NG/bin")));

    if (runtimeRoot.exists()) {
        QDirIterator it(runtimeRoot.absolutePath(),
                        QStringList{QStringLiteral("*.dll")},
                        QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            addDir(QFileInfo(it.next()).absolutePath());
        }
    }

    return dirs;
}

inline void crispPrependRuntimeDirsToPath(const QStringList& dirs)
{
    QStringList pathEntries = QString::fromLocal8Bit(qgetenv("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (auto it = dirs.crbegin(); it != dirs.crend(); ++it) {
        crispPrependPath(pathEntries, *it);
    }
    qputenv("PATH", pathEntries.join(QDir::listSeparator()).toLocal8Bit());
}

#ifdef Q_OS_WIN
inline int crispDllLoadPriority(const QString& fileName)
{
    const QString name = fileName.toLower();
    if (name == QStringLiteral("ggml.dll") || name == QStringLiteral("ggmk.dll")) return 0;
    if (name == QStringLiteral("ggml-base.dll") || name == QStringLiteral("ggmk-base.dll")) return 1;
    if (name.startsWith(QStringLiteral("ggml-")) || name.startsWith(QStringLiteral("ggmk-"))) return 2;
    if (name.startsWith(QStringLiteral("cudart")) || name.startsWith(QStringLiteral("cublas"))) return 3;
    if (name.contains(QStringLiteral("espeak"))) return 4;
    return 5;
}

inline QVector<HMODULE> crispPreloadRuntimeDlls(const QString& mainLibPath, const QStringList& dirs)
{
    const QString mainPath = QDir::toNativeSeparators(QDir::cleanPath(mainLibPath));
    QVector<QFileInfo> dlls;

    for (const QString& dir : dirs) {
        const QFileInfoList files = QDir(dir).entryInfoList(QStringList{QStringLiteral("*.dll")}, QDir::Files);
        for (const QFileInfo& file : files) {
            const QString filePath = QDir::toNativeSeparators(QDir::cleanPath(file.absoluteFilePath()));
            if (filePath.compare(mainPath, Qt::CaseInsensitive) == 0)
                continue;
            if (file.fileName().compare(QStringLiteral("whisper.dll"), Qt::CaseInsensitive) == 0)
                continue;
            dlls.append(file);
        }
    }

    std::sort(dlls.begin(), dlls.end(), [](const QFileInfo& a, const QFileInfo& b) {
        const int ap = crispDllLoadPriority(a.fileName());
        const int bp = crispDllLoadPriority(b.fileName());
        if (ap != bp) return ap < bp;
        return a.fileName().compare(b.fileName(), Qt::CaseInsensitive) < 0;
    });

    QSet<QString> loadedPaths;
    QVector<HMODULE> handles;
    for (const QFileInfo& dll : dlls) {
        const QString filePath = QDir::toNativeSeparators(QDir::cleanPath(dll.absoluteFilePath()));
        if (loadedPaths.contains(filePath))
            continue;
        loadedPaths.insert(filePath);

        HMODULE module = LoadLibraryW(reinterpret_cast<LPCWSTR>(filePath.utf16()));
        if (module) {
            handles.append(module);
        }
    }
    return handles;
}

inline void crispReleasePreloadedRuntimeDlls(QVector<HMODULE>& handles)
{
    for (auto it = handles.rbegin(); it != handles.rend(); ++it) {
        FreeLibrary(*it);
    }
    handles.clear();
}

inline void crispUnloadLibraryAndDependencies(QLibrary& library, QVector<HMODULE>& handles)
{
    if (library.isLoaded()) {
        library.unload();
    }
    crispReleasePreloadedRuntimeDlls(handles);
}
#endif

} // namespace LAStudio
