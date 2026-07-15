#include "WorkflowTranscript.h"

#include <QHash>
#include <QSet>

namespace LAStudio {

namespace {
void setError(QString *error, const QString &message) { if (error) *error = message; }
}

bool WorkflowTranscriptArtifact::validate(QString *error) const
{
    if (schemaVersion != CurrentSchemaVersion) {
        setError(error, QStringLiteral("Unsupported transcript artifact schema version: %1").arg(schemaVersion));
        return false;
    }
    QSet<QString> ids;
    for (int i = 0; i < segments.size(); ++i) {
        const QVariantMap segment = segments.at(i).toMap();
        const QString id = segment.value(QStringLiteral("id")).toString();
        if (id.isEmpty()) {
            setError(error, QStringLiteral("Transcript segment at index %1 has no stable id.").arg(i));
            return false;
        }
        if (ids.contains(id)) {
            setError(error, QStringLiteral("Duplicate transcript segment id: %1").arg(id));
            return false;
        }
        ids.insert(id);
        const qint64 start = segment.value(QStringLiteral("startMs")).toLongLong();
        const qint64 end = segment.value(QStringLiteral("endMs")).toLongLong();
        if (start < 0 || end <= start) {
            setError(error, QStringLiteral("Invalid transcript timing for segment: %1").arg(id));
            return false;
        }
        const QVariantList words = segment.value(QStringLiteral("words")).toList();
        qint64 previousWordEnd = start;
        for (int wordIndex = 0; wordIndex < words.size(); ++wordIndex) {
            const QVariantMap word = words.at(wordIndex).toMap();
            const QString text = word.value(QStringLiteral("text"),
                                            word.value(QStringLiteral("word"))).toString().trimmed();
            const qint64 wordStart = word.value(QStringLiteral("startMs")).toLongLong();
            const qint64 wordEnd = word.value(QStringLiteral("endMs")).toLongLong();
            if (text.isEmpty() || wordStart < start || wordEnd <= wordStart || wordEnd > end
                || wordStart < previousWordEnd) {
                setError(error, QStringLiteral("Invalid word timing at segment %1, word %2").arg(id).arg(wordIndex));
                return false;
            }
            previousWordEnd = wordEnd;
        }
    }
    return true;
}

bool WorkflowTranscriptArtifact::fromVariantList(const QVariantList &values,
                                                  WorkflowTranscriptArtifact &artifact,
                                                  QString *error)
{
    artifact = {};
    artifact.segments = values;
    return artifact.validate(error);
}

bool WorkflowTranscriptArtifact::mergePatches(const WorkflowTranscriptArtifact &base,
                                              const QVariantList &patches,
                                              WorkflowTranscriptArtifact &result,
                                              QString *error)
{
    if (!base.validate(error)) return false;
    result = base;
    QHash<QString, int> indexes;
    for (int i = 0; i < result.segments.size(); ++i)
        indexes.insert(result.segments.at(i).toMap().value(QStringLiteral("id")).toString(), i);

    QSet<QString> patchedIds;
    for (const QVariant &entry : patches) {
        const QVariantMap patch = entry.toMap();
        const QString id = patch.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || !indexes.contains(id)) {
            setError(error, QStringLiteral("Transcript patch references an unknown segment id: %1").arg(id));
            return false;
        }
        if (patchedIds.contains(id)) {
            setError(error, QStringLiteral("Transcript contains duplicate patch for segment id: %1").arg(id));
            return false;
        }
        patchedIds.insert(id);
        QVariantMap current = result.segments.at(indexes.value(id)).toMap();
        for (auto it = patch.cbegin(); it != patch.cend(); ++it) current.insert(it.key(), it.value());
        result.segments[indexes.value(id)] = current;
    }
    return result.validate(error);
}

} // namespace LAStudio
