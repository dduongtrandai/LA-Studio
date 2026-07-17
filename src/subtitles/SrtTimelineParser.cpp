#include "SrtTimelineParser.h"

#include <QFile>
#include <QRegularExpression>

namespace LAStudio {
namespace {

qint64 parseTimestamp(const QString &value)
{
    static const QRegularExpression re(
        QStringLiteral("^(\\d{1,2}):(\\d{2}):(\\d{2})[,.](\\d{1,3})$"));
    const auto match = re.match(value.trimmed());
    if (!match.hasMatch()) return -1;
    QString millis = match.captured(4);
    while (millis.size() < 3) millis.append(QLatin1Char('0'));
    return (match.captured(1).toLongLong() * 3600LL
            + match.captured(2).toLongLong() * 60LL
            + match.captured(3).toLongLong()) * 1000LL
        + millis.left(3).toLongLong();
}

} // namespace

SubtitleParseResult SrtTimelineParser::parseFile(const QString &path)
{
    SubtitleParseResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    return parseSrt(QString::fromUtf8(file.readAll()));
}

SubtitleParseResult SrtTimelineParser::parseSrt(const QString &content)
{
    SubtitleParseResult result;
    QString normalized = content;
    normalized.remove(QChar::ByteOrderMark);
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (normalized.trimmed().isEmpty()) {
        result.error = QStringLiteral("The SRT file is empty.");
        return result;
    }

    const QStringList blocks = normalized.split(
        QRegularExpression(QStringLiteral("\\n\\s*\\n")), Qt::SkipEmptyParts);
    const QRegularExpression timing(QStringLiteral(
        "^\\s*(\\d{1,2}:\\d{2}:\\d{2}[,.]\\d{1,3})\\s*-->\\s*"
        "(\\d{1,2}:\\d{2}:\\d{2}[,.]\\d{1,3})"));
    int fallbackNumber = 1;
    int cueIndex = 0;
    for (const QString &block : blocks) {
        const QStringList lines = block.split(QLatin1Char('\n'));
        int timingIndex = -1;
        QRegularExpressionMatch match;
        for (int i = 0; i < lines.size(); ++i) {
            match = timing.match(lines.at(i));
            if (match.hasMatch()) { timingIndex = i; break; }
        }
        if (timingIndex < 0) { ++result.skippedCues; continue; }

        const qint64 start = parseTimestamp(match.captured(1));
        const qint64 end = parseTimestamp(match.captured(2));
        QStringList body;
        for (int i = timingIndex + 1; i < lines.size(); ++i) {
            const QString line = lines.at(i).trimmed();
            if (!line.isEmpty()) body.append(line);
        }
        if (start < 0 || end <= start || body.isEmpty()) {
            ++result.skippedCues;
            continue;
        }

        int number = fallbackNumber;
        if (timingIndex > 0) {
            bool parsed = false;
            const int candidate = lines.at(timingIndex - 1).trimmed().toInt(&parsed);
            if (parsed) number = candidate;
        }
        TimedTextCue cue;
        cue.id = QStringLiteral("cue-%1").arg(++cueIndex);
        cue.cueNumber = number;
        cue.text = body.join(QLatin1Char('\n'));
        cue.startMs = start;
        cue.endMs = end;
        result.cues.append(cue);
        fallbackNumber = number + 1;
    }

    if (result.cues.isEmpty()) {
        result.error = QStringLiteral("No valid subtitle cues were found.");
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace LAStudio
