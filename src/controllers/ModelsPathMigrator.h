#pragma once

#include <QString>

namespace LAStudio {

class ModelsPathMigrator {
public:
    static bool copyDirectoryMerge(const QString &srcDir, const QString &dstDir, QString &errorMsg);
    static bool removeDirectory(const QString &dirPath);
    static bool filesAreIdentical(const QString &srcPath, const QString &dstPath, QString &errorMsg);
};

} // namespace LAStudio
