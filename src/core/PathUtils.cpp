#include "PathUtils.h"
#include <QStandardPaths>
#include <QDir>
#include <QUrl>

namespace LAStudio {

QString PathUtils::dataDir()
{
    return QDir::homePath() + QStringLiteral("/.lastudio");
}

QString PathUtils::modelsDir()
{
    return dataDir() + QStringLiteral("/models");
}

QString PathUtils::hubModelsDir()
{
    return dataDir() + QStringLiteral("/hub/models");
}

QString PathUtils::cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

QString PathUtils::logsDir()
{
    return dataDir() + QStringLiteral("/logs");
}

QString PathUtils::extensionsDir()
{
    return dataDir() + QStringLiteral("/extensions");
}

QString PathUtils::backendsDir()
{
    return extensionsDir() + QStringLiteral("/backends");
}

void PathUtils::ensureDirsExist()
{
    QDir().mkpath(dataDir());
    QDir().mkpath(hubModelsDir());
    QDir().mkpath(modelsDir());
    QDir().mkpath(cacheDir());
    QDir().mkpath(logsDir());
    QDir().mkpath(extensionsDir());
    QDir().mkpath(backendsDir());
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <QVarLengthArray>
#endif

QString PathUtils::toNativeShortPath(const QString &longPath)
{
#ifdef Q_OS_WIN
    if (longPath.isEmpty())
        return longPath;

    QString nativePath = QDir::toNativeSeparators(longPath);
    DWORD length = GetShortPathNameW(reinterpret_cast<const wchar_t*>(nativePath.utf16()), nullptr, 0);
    if (length == 0) {
        return longPath;
    }

    QVarLengthArray<wchar_t, MAX_PATH> buffer(static_cast<int>(length));
    DWORD result = GetShortPathNameW(reinterpret_cast<const wchar_t*>(nativePath.utf16()), buffer.data(), length);
    if (result > 0 && result < length) {
        return QDir::fromNativeSeparators(QString::fromWCharArray(buffer.constData(), static_cast<int>(result)));
    }
#endif
    return longPath;
}

QString PathUtils::urlToLocalPath(const QString &urlStr)
{
    QUrl url(urlStr);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return urlStr;
}

} // namespace LAStudio

