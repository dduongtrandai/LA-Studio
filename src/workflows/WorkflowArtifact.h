#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

namespace LAStudio {

struct WorkflowArtifactReference
{
    QString artifactId;
    QString typeId;
    int schemaVersion = 1;
    QString contentHash;
    QString relativePath;
    qint64 sizeBytes = 0;
    QVariantMap metadata;

    bool isValid() const { return !artifactId.isEmpty() && !typeId.isEmpty() && !contentHash.isEmpty(); }
    QJsonObject toJson() const;
    static WorkflowArtifactReference fromJson(const QJsonObject &json);
};

class WorkflowArtifactStore final
{
public:
    explicit WorkflowArtifactStore(QString rootPath);

    QString rootPath() const { return m_rootPath; }
    QString createStagingDirectory(const QString &runId, const QString &nodeRunId, QString *error = nullptr) const;
    bool commitFile(const QString &stagingFile,
                   const QString &typeId,
                   const QString &runId,
                   const QString &nodeRunId,
                   WorkflowArtifactReference &reference,
                   QString *error = nullptr) const;
    QString resolve(const WorkflowArtifactReference &reference) const;

private:
    QString m_rootPath;
};

} // namespace LAStudio
