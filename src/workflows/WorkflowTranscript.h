#pragma once

#include <QVariantList>
#include <QString>

namespace LAStudio {

struct WorkflowTranscriptArtifact
{
    static constexpr int CurrentSchemaVersion = 1;

    int schemaVersion = CurrentSchemaVersion;
    QString artifactId;
    QString revisionId;
    QString derivedFrom;
    QVariantList segments;

    bool validate(QString *error = nullptr) const;
    QVariantList toVariantList() const { return segments; }
    static bool fromVariantList(const QVariantList &values, WorkflowTranscriptArtifact &artifact,
                                QString *error = nullptr);
    static bool mergePatches(const WorkflowTranscriptArtifact &base,
                             const QVariantList &patches,
                             WorkflowTranscriptArtifact &result,
                             QString *error = nullptr);
};

} // namespace LAStudio
