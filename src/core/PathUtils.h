#pragma once

#include <QString>

namespace LAStudio {

class PathUtils {
public:
    static QString dataDir();
    static QString modelsDir();
    static QString hubModelsDir();
    static QString cacheDir();
    static QString logsDir();
    static QString extensionsDir();
    static QString backendsDir();
    static QString toNativeShortPath(const QString &longPath);
    static QString urlToLocalPath(const QString &urlStr);
    static void ensureDirsExist();
};

} // namespace LAStudio

