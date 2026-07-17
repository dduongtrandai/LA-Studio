#include "TranslationProject.h"

#include "subtitles/SrtTimelineParser.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace LAStudio {
namespace {
void setError(QString *error, const QString &text) { if (error) *error = text; }
QString segmentId(int index) { return QStringLiteral("segment-%1").arg(index + 1); }
qint64 timestampToMs(const QString &value) {
    const QRegularExpression re(QStringLiteral("^(?:(\\d+):)?(\\d{2}):(\\d{2})[,.](\\d{3})$"));
    const auto match = re.match(value.trimmed());
    if (!match.hasMatch()) return -1;
    return ((match.captured(1).isEmpty() ? 0 : match.captured(1).toLongLong()) * 3600 + match.captured(2).toLongLong() * 60 + match.captured(3).toLongLong()) * 1000 + match.captured(4).toLongLong();
}
QString msToTimestamp(qint64 ms, bool vtt) {
    const qint64 hours = ms / 3600000; ms %= 3600000;
    const qint64 minutes = ms / 60000; ms %= 60000;
    const qint64 seconds = ms / 1000; ms %= 1000;
    return QStringLiteral("%1:%2:%3%4%5").arg(hours, 2, 10, QLatin1Char('0')).arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0')).arg(vtt ? QLatin1Char('.') : QLatin1Char(',')).arg(ms, 3, 10, QLatin1Char('0'));
}
QJsonObject toJsonSegment(const QVariantMap &segment) { return QJsonObject::fromVariantMap(segment); }
}

bool TranslationProject::save(QString *error) const
{
    if (projectPath.isEmpty()) { setError(error, QStringLiteral("Choose a project path before saving.")); return false; }
    QJsonObject root{{QStringLiteral("schemaVersion"), CurrentSchemaVersion}, {QStringLiteral("sourcePath"), sourcePath}, {QStringLiteral("sourceFormat"), sourceFormat}, {QStringLiteral("sourceLanguage"), sourceLanguage}, {QStringLiteral("targetLanguage"), targetLanguage}};
    QJsonArray entries; for (const QVariant &segment : segments) entries.append(toJsonSegment(segment.toMap())); root.insert(QStringLiteral("segments"), entries);
    QSaveFile file(projectPath); if (!file.open(QIODevice::WriteOnly)) { setError(error, file.errorString()); return false; }
    file.write(QJsonDocument(root).toJson()); if (!file.commit()) { setError(error, file.errorString()); return false; } return true;
}

bool TranslationProject::load(const QString &path, TranslationProject &project, QString *error)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) { setError(error, file.errorString()); return false; }
    QJsonParseError parseError; const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) { setError(error, QStringLiteral("Invalid Translation project JSON.")); return false; }
    const QJsonObject root = document.object(); if (root.value(QStringLiteral("schemaVersion")).toInt() != CurrentSchemaVersion) { setError(error, QStringLiteral("Unsupported Translation project schema.")); return false; }
    TranslationProject loaded; loaded.projectPath = QFileInfo(path).absoluteFilePath(); loaded.sourcePath = root.value(QStringLiteral("sourcePath")).toString(); loaded.sourceFormat = root.value(QStringLiteral("sourceFormat")).toString(QStringLiteral("text")); loaded.sourceLanguage = root.value(QStringLiteral("sourceLanguage")).toString(QStringLiteral("en")); loaded.targetLanguage = root.value(QStringLiteral("targetLanguage")).toString(QStringLiteral("vi"));
    const QJsonArray entries = root.value(QStringLiteral("segments")).toArray(); for (const QJsonValue &entry : entries) { const QVariantMap segment = entry.toObject().toVariantMap(); if (segment.value(QStringLiteral("id")).toString().isEmpty()) { setError(error, QStringLiteral("Project has a segment without an id.")); return false; } loaded.segments.append(segment); }
    project = std::move(loaded); return true;
}

bool TranslationProject::importText(const QString &text, TranslationProject &project, QString *error)
{
    const QStringList paragraphs = text.split(QRegularExpression(QStringLiteral("\\r?\\n\\s*\\r?\\n")), Qt::SkipEmptyParts);
    QVariantList segments; int index = 0; for (const QString &paragraph : paragraphs) { const QString source = paragraph.trimmed(); if (!source.isEmpty()) segments.append(QVariantMap{{QStringLiteral("id"), segmentId(index++)}, {QStringLiteral("sourceText"), source}, {QStringLiteral("targetText"), QString()}, {QStringLiteral("state"), QStringLiteral("ready")}}); }
    if (segments.isEmpty()) { setError(error, QStringLiteral("The text contains no translatable paragraphs.")); return false; }
    project.sourceFormat = QStringLiteral("text"); project.segments = segments; return true;
}

bool TranslationProject::importSubtitle(const QString &text, const QString &format, TranslationProject &project, QString *error)
{
    if (format == QStringLiteral("srt")) {
        const SubtitleParseResult parsed = SrtTimelineParser::parseSrt(text);
        if (!parsed.ok || parsed.skippedCues > 0) {
            setError(error, parsed.error.isEmpty() ? QStringLiteral("Invalid subtitle cue.") : parsed.error);
            return false;
        }
        QVariantList segments;
        for (const TimedTextCue &cue : parsed.cues) {
            segments.append(QVariantMap{{QStringLiteral("id"), segmentId(segments.size())},
                                        {QStringLiteral("sourceText"), cue.text},
                                        {QStringLiteral("targetText"), QString()},
                                        {QStringLiteral("startMs"), cue.startMs},
                                        {QStringLiteral("endMs"), cue.endMs},
                                        {QStringLiteral("state"), QStringLiteral("ready")}});
        }
        if (segments.isEmpty()) {
            setError(error, QStringLiteral("No subtitle cues were found."));
            return false;
        }
        project.sourceFormat = format;
        project.segments = segments;
        return true;
    }

    QString normalized = text; normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (format == QStringLiteral("vtt")) { if (!normalized.startsWith(QStringLiteral("WEBVTT"))) { setError(error, QStringLiteral("Invalid VTT header.")); return false; } normalized = normalized.mid(normalized.indexOf('\n') + 1); }
    const QStringList blocks = normalized.split(QRegularExpression(QStringLiteral("\\n\\s*\\n")), Qt::SkipEmptyParts);
    QVariantList segments; int index = 0; const QRegularExpression timing(QStringLiteral("(\\d{1,2}:\\d{2}:\\d{2}[,.]\\d{3})\\s*-->\\s*(\\d{1,2}:\\d{2}:\\d{2}[,.]\\d{3})"));
    for (const QString &block : blocks) {
        QStringList lines = block.split('\n'); int timingLine = -1; QRegularExpressionMatch match;
        for (int i = 0; i < lines.size(); ++i) { match = timing.match(lines.at(i)); if (match.hasMatch()) { timingLine = i; break; } }
        if (timingLine < 0) continue;
        const qint64 startMs = timestampToMs(match.captured(1)); const qint64 endMs = timestampToMs(match.captured(2)); const QString source = lines.mid(timingLine + 1).join(QStringLiteral("\n")).trimmed();
        if (startMs < 0 || endMs < startMs || source.isEmpty()) { setError(error, QStringLiteral("Invalid subtitle cue.")); return false; }
        segments.append(QVariantMap{{QStringLiteral("id"), segmentId(index++)}, {QStringLiteral("sourceText"), source}, {QStringLiteral("targetText"), QString()}, {QStringLiteral("startMs"), startMs}, {QStringLiteral("endMs"), endMs}, {QStringLiteral("state"), QStringLiteral("ready")}});
    }
    if (segments.isEmpty()) { setError(error, QStringLiteral("No subtitle cues were found.")); return false; }
    project.sourceFormat = format; project.segments = segments; return true;
}

QString TranslationProject::exportText() const { QStringList output; for (const QVariant &entry : segments) output.append(entry.toMap().value(QStringLiteral("targetText")).toString()); return output.join(QStringLiteral("\n\n")); }
QString TranslationProject::exportSubtitle(QString *error) const { const bool vtt = sourceFormat == QStringLiteral("vtt"); if (sourceFormat != QStringLiteral("srt") && !vtt) { setError(error, QStringLiteral("This project is not a subtitle project.")); return {}; } QStringList lines; if (vtt) lines.append(QStringLiteral("WEBVTT\n")); int number = 1; for (const QVariant &entry : segments) { const QVariantMap segment = entry.toMap(); const QString target = segment.value(QStringLiteral("targetText")).toString().trimmed(); if (target.isEmpty()) { setError(error, QStringLiteral("Translate every segment before exporting subtitles.")); return {}; } if (!vtt) lines.append(QString::number(number)); lines.append(msToTimestamp(segment.value(QStringLiteral("startMs")).toLongLong(), vtt) + QStringLiteral(" --> ") + msToTimestamp(segment.value(QStringLiteral("endMs")).toLongLong(), vtt)); lines.append(target); lines.append(QString()); ++number; } return lines.join(QStringLiteral("\n")); }
} // namespace LAStudio
