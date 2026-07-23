#include "DictionaryStore.h"

#include <QFile>
#include <QTextStream>

namespace vietnorm::detail {
namespace {

QStringList parseCsvLine(const QString &line)
{
    QStringList fields;
    QString field;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                field += QLatin1Char('"');
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (ch == QLatin1Char(',') && !quoted) {
            fields.append(field.trimmed());
            field.clear();
        } else {
            field += ch;
        }
    }
    fields.append(field.trimmed());
    return fields;
}

} // namespace

DictionaryStore DictionaryStore::builtIn()
{
    DictionaryStore result;
    Q_INIT_RESOURCE(vietnorm_data);
    QString error;
    if (!loadCsv(QStringLiteral(":/vietnorm/data/acronyms.csv"), true, result, &error)
        || !loadCsv(QStringLiteral(":/vietnorm/data/non-vietnamese-words.csv"), false, result, &error)) {
        result.acronyms.insert(QStringLiteral("ubnd"), QStringLiteral("ủy ban nhân dân"));
        result.acronyms.insert(QStringLiteral("nasa"), QStringLiteral("na-sa"));
        result.foreignWords.insert(QStringLiteral("container"), QStringLiteral("công-tê-nơ"));
        result.foreignWords.insert(QStringLiteral("hello"), QStringLiteral("hê-lô"));
    }
    return result;
}

bool DictionaryStore::loadCsv(const QString &path, bool acronymFile,
                               DictionaryStore &store, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open dictionary: %1").arg(path);
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    bool header = true;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (header) { header = false; continue; }
        if (line.trimmed().isEmpty()) continue;
        const QStringList fields = parseCsvLine(line);
        if (fields.size() < 2 || fields.at(0).trimmed().isEmpty()) continue;
        const QString key = fields.at(0).trimmed().toLower();
        const QString value = fields.at(1).trimmed();
        if (acronymFile) store.acronyms.insert(key, value);
        else store.foreignWords.insert(key, value);
    }
    return true;
}

bool DictionaryStore::loadDirectory(const QString &directory,
                                     DictionaryStore &store, QString *error)
{
    DictionaryStore loaded = builtIn();
    QString localError;
    if (!loadCsv(directory + QStringLiteral("/acronyms.csv"), true, loaded, &localError)
        || !loadCsv(directory + QStringLiteral("/non-vietnamese-words.csv"), false, loaded, &localError)) {
        if (error) *error = localError;
        return false;
    }
    store = std::move(loaded);
    return true;
}

} // namespace vietnorm::detail
