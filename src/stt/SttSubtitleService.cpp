#include "SttSubtitleService.h"

#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QtMath>
#include <algorithm>

namespace LAStudio {

namespace {

qint64 startMs(const QVariantMap &map)
{
    if (map.contains(QStringLiteral("startMs"))) return map.value(QStringLiteral("startMs")).toLongLong();
    return qRound64(map.value(QStringLiteral("start")).toDouble() * 1000.0);
}

qint64 endMs(const QVariantMap &map, qint64 fallback)
{
    if (map.contains(QStringLiteral("endMs"))) return map.value(QStringLiteral("endMs")).toLongLong();
    const double end = map.value(QStringLiteral("end")).toDouble();
    return end > 0.0 ? qRound64(end * 1000.0) : fallback;
}

QString textOf(const QVariantMap &map)
{
    return map.value(QStringLiteral("text"),
                    map.value(QStringLiteral("sourceText"),
                              map.value(QStringLiteral("transcript")))).toString().simplified();
}

QString timestamp(qint64 ms, bool vtt)
{
    ms = qMax<qint64>(0, ms);
    const qint64 h = ms / 3600000;
    const int m = int((ms / 60000) % 60);
    const int s = int((ms / 1000) % 60);
    const int milli = int(ms % 1000);
    return QStringLiteral("%1:%2:%3%4%5")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(vtt ? QLatin1Char('.') : QLatin1Char(','))
        .arg(milli, 3, 10, QLatin1Char('0'));
}

QString cueText(const QVariantMap &cue)
{
    QString text = textOf(cue);
    text.replace(QRegularExpression(QStringLiteral("\\s*\\r?\\n\\s*")), QStringLiteral("\n"));
    return text.trimmed();
}

QString render(const QVariantList &cues, bool vtt)
{
    QString output = vtt ? QStringLiteral("WEBVTT\n\n") : QString();
    int index = 1;
    for (const QVariant &entry : cues) {
        const QVariantMap cue = entry.toMap();
        const qint64 start = startMs(cue);
        const qint64 end = endMs(cue, start + 1000);
        const QString text = cueText(cue);
        if (text.isEmpty() || end <= start) continue;
        if (!vtt) output += QString::number(index++) + QStringLiteral("\n");
        output += timestamp(start, vtt) + QStringLiteral(" --> ") + timestamp(end, vtt) + QStringLiteral("\n");
        output += text + QStringLiteral("\n\n");
    }
    return output;
}

bool write(const QString &path, const QString &content, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Could not open subtitle output: %1").arg(path);
        return false;
    }
    file.write(content.toUtf8());
    if (!file.commit()) {
        if (error) *error = QStringLiteral("Could not commit subtitle output: %1").arg(path);
        return false;
    }
    return true;
}

} // namespace

QVariantList SttSubtitleService::cuesFromSegments(const QVariantList &segments, qint64 durationMs)
{
    QVariantList result;
    int index = 1;
    for (const QVariant &entry : segments) {
        const QVariantMap source = entry.toMap();
        QString text = textOf(source);
        if (text.isEmpty()) continue;
        const qint64 sourceStart = qMax<qint64>(0, startMs(source));
        const qint64 sourceEnd = qMax(sourceStart + 1, endMs(source, durationMs > sourceStart ? durationMs : sourceStart + 1000));
        const QVariantList words = source.value(QStringLiteral("words")).toList();
        if (words.isEmpty() || sourceEnd - sourceStart <= 7000) {
            QVariantMap cue = source;
            cue.insert(QStringLiteral("id"), cue.value(QStringLiteral("id"), QStringLiteral("cue-%1").arg(index++)));
            cue.insert(QStringLiteral("startMs"), sourceStart);
            cue.insert(QStringLiteral("endMs"), sourceEnd);
            cue.insert(QStringLiteral("text"), text);
            cue.insert(QStringLiteral("timingSource"), cue.value(QStringLiteral("timingSource"), QStringLiteral("asr")));
            result.append(cue);
            continue;
        }

        QStringList parts;
        qint64 cueStart = sourceStart;
        qint64 lastEnd = sourceStart;
        auto flush = [&]() {
            if (parts.isEmpty()) return;
            const QString cueTextValue = parts.join(QLatin1Char(' ')).simplified();
            QVariantMap out = source;
            out.insert(QStringLiteral("id"), QStringLiteral("cue-%1").arg(index++));
            out.insert(QStringLiteral("startMs"), cueStart);
            out.insert(QStringLiteral("endMs"), qMax(cueStart + 1, lastEnd));
            out.insert(QStringLiteral("text"), cueTextValue);
            out.insert(QStringLiteral("timingSource"), out.value(QStringLiteral("timingSource"), QStringLiteral("asr")));
            result.append(out);
            parts.clear();
            cueStart = lastEnd;
        };
        for (const QVariant &wordEntry : words) {
            const QVariantMap word = wordEntry.toMap();
            const QString wordText = word.value(QStringLiteral("text"), word.value(QStringLiteral("word"))).toString().trimmed();
            if (wordText.isEmpty()) continue;
            const qint64 wordStart = qMax(sourceStart, startMs(word));
            const qint64 wordEnd = qMax(wordStart + 1, endMs(word, wordStart + 250));
            const int chars = parts.join(QLatin1Char(' ')).size() + (parts.isEmpty() ? 0 : 1) + wordText.size();
            if (!parts.isEmpty() && (wordEnd - cueStart > 7000 || chars > 84)) flush();
            if (parts.isEmpty()) cueStart = wordStart;
            parts.append(wordText);
            lastEnd = wordEnd;
        }
        flush();
    }
    std::stable_sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return startMs(a.toMap()) < startMs(b.toMap());
    });
    return result;
}

QString SttSubtitleService::toSrt(const QVariantList &cues) { return render(cues, false); }
QString SttSubtitleService::toVtt(const QVariantList &cues) { return render(cues, true); }
bool SttSubtitleService::writeSrt(const QString &path, const QVariantList &cues, QString *error)
{ return write(path, toSrt(cues), error); }
bool SttSubtitleService::writeVtt(const QString &path, const QVariantList &cues, QString *error)
{ return write(path, toVtt(cues), error); }

} // namespace LAStudio
