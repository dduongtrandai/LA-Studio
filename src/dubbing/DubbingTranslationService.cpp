#include "dubbing/DubbingTranslationService.h"

#include "core/ModelManager.h"
#include "core/RuntimeManager.h"
#include "runtimes/CrispTranslationInterface.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace LAStudio {

namespace {
void setError(QString *error, const QString &message) { if (error) *error = message; }

QString findModelPath(const QVariantMap &model)
{
    const QString modelDir = model.value(QStringLiteral("path")).toString();
    for (const QString &file : model.value(QStringLiteral("files")).toStringList()) {
        if (!file.endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) continue;
        const QString path = QDir(modelDir).absoluteFilePath(file);
        if (QFileInfo::exists(path)) return path;
    }
    if (QFileInfo(modelDir).isFile() && modelDir.endsWith(QStringLiteral(".gguf"), Qt::CaseInsensitive)) return modelDir;
    if (!modelDir.isEmpty()) {
        QDirIterator it(modelDir, QStringList{QStringLiteral("*.gguf")}, QDir::Files);
        if (it.hasNext()) return it.next();
    }
    return {};
}
}

bool DubbingTranslationService::prepare(const QString &sourceLanguage, const QString &targetLanguage,
                                        DubbingTranslationRequest &request, QString *error) const
{
    return m_service.prepareFallback(sourceLanguage, targetLanguage, request, error);
}

} // namespace LAStudio
