#include "WorkflowArtifact.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QRegularExpression>

namespace LAStudio {

namespace {
void setError(QString *error, const QString &message)
{
    if (error) *error = message;
}

QString safeId(const QString &value)
{
    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) return QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString safe = normalized;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    safe.replace(QLatin1Char('.'), QLatin1Char('_'));
    return safe;
}
}

QJsonObject WorkflowArtifactReference::toJson() const
{
    return QJsonObject{
        {QStringLiteral("artifactId"), artifactId},
        {QStringLiteral("type"), typeId},
        {QStringLiteral("schemaVersion"), schemaVersion},
        {QStringLiteral("contentHash"), contentHash},
        {QStringLiteral("relativePath"), relativePath},
        {QStringLiteral("sizeBytes"), sizeBytes},
        {QStringLiteral("metadata"), QJsonObject::fromVariantMap(metadata)}
    };
}

WorkflowArtifactReference WorkflowArtifactReference::fromJson(const QJsonObject &json)
{
    WorkflowArtifactReference result;
    result.artifactId = json.value(QStringLiteral("artifactId")).toString();
    result.typeId = json.value(QStringLiteral("type")).toString();
    result.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(1);
    result.contentHash = json.value(QStringLiteral("contentHash")).toString();
    result.relativePath = json.value(QStringLiteral("relativePath")).toString();
    result.sizeBytes = json.value(QStringLiteral("sizeBytes")).toVariant().toLongLong();
    result.metadata = json.value(QStringLiteral("metadata")).toObject().toVariantMap();
    return result;
}

WorkflowArtifactStore::WorkflowArtifactStore(QString rootPath)
    : m_rootPath(QDir::cleanPath(std::move(rootPath))) {}

QString WorkflowArtifactStore::createStagingDirectory(const QString &runId, const QString &nodeRunId, QString *error) const
{
    const QString path = QDir(m_rootPath).filePath(QStringLiteral("staging/%1/%2/%3")
                                                    .arg(safeId(runId), safeId(nodeRunId),
                                                         QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(path)) {
        setError(error, QStringLiteral("Cannot create artifact staging directory: %1").arg(path));
        return {};
    }
    return path;
}

bool WorkflowArtifactStore::commitFile(const QString &stagingFile,
                                       const QString &typeId,
                                       const QString &runId,
                                       const QString &nodeRunId,
                                       WorkflowArtifactReference &reference,
                                       QString *error) const
{
    QFile source(stagingFile);
    if (!source.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Cannot open staged artifact: %1").arg(source.errorString()));
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (chunk.isEmpty() && !source.atEnd()) {
            setError(error, QStringLiteral("Cannot read staged artifact: %1").arg(source.errorString()));
            return false;
        }
        hash.addData(chunk);
    }
    const QString contentHash = QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
    const QString artifactId = contentHash;
    const QString relativePath = QStringLiteral("artifacts/%1.bin").arg(QString::fromLatin1(hash.result().toHex()));
    const QString destination = QDir(m_rootPath).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        setError(error, QStringLiteral("Cannot create artifact directory: %1").arg(QFileInfo(destination).absolutePath()));
        return false;
    }
    if (!QFileInfo::exists(destination)) {
        if (!source.seek(0)) {
            setError(error, QStringLiteral("Cannot rewind staged artifact."));
            return false;
        }
        QSaveFile output(destination);
        if (!output.open(QIODevice::WriteOnly)) {
            setError(error, QStringLiteral("Cannot stage artifact commit: %1").arg(output.errorString()));
            return false;
        }
        while (!source.atEnd()) {
            const QByteArray chunk = source.read(1024 * 1024);
            if (chunk.isEmpty() && !source.atEnd()) {
                setError(error, QStringLiteral("Cannot read staged artifact: %1").arg(source.errorString()));
                return false;
            }
            if (output.write(chunk) != chunk.size()) {
                setError(error, QStringLiteral("Cannot write committed artifact: %1").arg(output.errorString()));
                return false;
            }
        }
        if (!output.commit()) {
            setError(error, QStringLiteral("Cannot atomically commit artifact: %1").arg(output.errorString()));
            return false;
        }
    }
    reference.artifactId = artifactId;
    reference.typeId = typeId;
    reference.schemaVersion = 1;
    reference.contentHash = contentHash;
    reference.relativePath = relativePath;
    reference.sizeBytes = QFileInfo(stagingFile).size();
    reference.metadata = {{QStringLiteral("runId"), runId}, {QStringLiteral("nodeRunId"), nodeRunId}};
    source.close();
    QFile::remove(stagingFile);
    return true;
}

QString WorkflowArtifactStore::resolve(const WorkflowArtifactReference &reference) const
{
    if (!reference.isValid() || reference.relativePath.contains(QStringLiteral(".."))
        || QDir::isAbsolutePath(reference.relativePath)) return {};
    const QString path = QDir(m_rootPath).filePath(reference.relativePath);
    return QFileInfo::exists(path) ? QFileInfo(path).absoluteFilePath() : QString();
}

} // namespace LAStudio
