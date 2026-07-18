#include "FileAccessService.h"
#include "core/PathUtils.h"
#include "core/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>

namespace LAStudio {

FileAccessService::FileAccessService(QObject *parent)
    : QObject(parent)
{
}

QString FileAccessService::urlToLocalPath(const QString &urlStr) const
{
    return PathUtils::urlToLocalPath(urlStr);
}

QString FileAccessService::readTextFile(const QString &path) const
{
    QString localPath = urlToLocalPath(path);
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::error(QStringLiteral("FileAccessService"), QStringLiteral("Failed to open file: %1").arg(localPath));
        return QString();
    }
    QTextStream in(&file);
    return in.readAll();
}

bool FileAccessService::writeTextFile(const QString &path, const QString &content) const
{
    QString localPath = urlToLocalPath(path);
    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::error(QStringLiteral("FileAccessService"), QStringLiteral("Failed to open file for writing: %1").arg(localPath));
        return false;
    }
    QTextStream out(&file);
    out << content;
    return true;
}

bool FileAccessService::localFileExists(const QString &path) const
{
    return QFileInfo::exists(QDir::fromNativeSeparators(path));
}

} // namespace LAStudio
