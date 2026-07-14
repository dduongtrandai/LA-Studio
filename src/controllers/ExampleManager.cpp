#include "ExampleManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <utility>

namespace LAStudio {
namespace {

QString sourceDir()
{
#ifdef LASTUDIO_SOURCE_DIR
    return QStringLiteral(LASTUDIO_SOURCE_DIR);
#else
    return QString();
#endif
}

QStringList stringListFromVariant(const QVariant &value)
{
    QStringList result;
    const QVariantList list = value.toList();
    for (const QVariant &item : list) {
        const QString text = item.toString();
        if (!text.isEmpty())
            result.append(text);
    }
    return result;
}

QSet<QString> languageSetFromFamily(const QVariantMap &family)
{
    QSet<QString> result;
    const auto addLanguages = [&result](const QVariantList &items) {
        for (const QVariant &itemValue : items) {
            const QVariantMap item = itemValue.toMap();
            const QString value = item.value(QStringLiteral("value")).toString();
            if (!value.isEmpty())
                result.insert(value);
        }
    };
    addLanguages(family.value(QStringLiteral("supportedLanguages")).toList());
    addLanguages(family.value(QStringLiteral("featuredLanguages")).toList());
    return result;
}

} // namespace

ExampleManager::ExampleManager(QObject *parent)
    : QObject(parent)
{
}

QVariantList ExampleManager::examplesForTask(const QString &task, const QVariantMap &family) const
{
    QVariantList result;
    const QVariantList examples = loadExamples();
    for (const QVariant &exampleValue : examples) {
        const QVariantMap example = exampleValue.toMap();
        if (example.value(QStringLiteral("task")).toString() != task)
            continue;
        if (!matchesFamily(example, family))
            continue;
        result.append(resolveExample(example));
    }
    return result;
}

QVariantMap ExampleManager::resolveExample(const QVariantMap &example) const
{
    QVariantMap resolved = example;
    QVariantMap inputs = resolved.value(QStringLiteral("inputs")).toMap();
    const QString referenceAudio = inputs.value(QStringLiteral("referenceAudio")).toString();
    if (!referenceAudio.isEmpty()) {
        const QString path = resolveLocalPath(referenceAudio);
        if (!path.isEmpty())
            inputs.insert(QStringLiteral("referenceAudioPath"), path);
    }
    resolved.insert(QStringLiteral("inputs"), inputs);
    return resolved;
}

QVariantList ExampleManager::loadExamples() const
{
    QVariantList examples;
    scanDirectory(QStringLiteral(":/examples"), examples);
    scanDirectory(QStringLiteral(":/LAStudio/examples"), examples);
    scanDirectory(QStringLiteral(":/qt/qml/LAStudio/examples"), examples);

    const QString src = sourceDir();
    if (!src.isEmpty())
        scanDirectory(QDir(src).absoluteFilePath(QStringLiteral("examples")), examples);

    return examples;
}

void ExampleManager::scanDirectory(const QString &rootPath, QVariantList &examples) const
{
    QDir root(rootPath);
    if (!root.exists())
        return;

    QSet<QString> seenIds;
    for (const QVariant &value : std::as_const(examples)) {
        const QString id = value.toMap().value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
            seenIds.insert(id);
    }

    QDirIterator it(rootPath, QStringList() << QStringLiteral("*.json"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        QVariantMap example = doc.object().toVariantMap();
        const QString id = example.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || seenIds.contains(id))
            continue;

        example.insert(QStringLiteral("sourcePath"), filePath);
        examples.append(example);
        seenIds.insert(id);
    }
}

bool ExampleManager::matchesFamily(const QVariantMap &example, const QVariantMap &family) const
{
    const QVariantMap model = example.value(QStringLiteral("model")).toMap();
    const QString exampleFamilyId = model.value(QStringLiteral("familyId")).toString();
    const bool modelRequired = model.value(QStringLiteral("required")).toBool();
    const QString familyId = family.value(QStringLiteral("id")).toString();

    if (modelRequired)
        return !familyId.isEmpty() && familyId == exampleFamilyId;

    if (exampleFamilyId.isEmpty())
        return true;
    if (!familyId.isEmpty() && familyId == exampleFamilyId)
        return true;

    const QStringList exampleCapabilities = stringListFromVariant(example.value(QStringLiteral("capabilities")));
    const QStringList familyCapabilities = stringListFromVariant(family.value(QStringLiteral("capabilities")));
    const QSet<QString> familyLanguages = languageSetFromFamily(family);

    for (const QString &capability : exampleCapabilities) {
        if (capability == QStringLiteral("tts") || capability == QStringLiteral("voice-cloning"))
            continue;
        if (capability.size() == 2 && familyLanguages.contains(capability))
            continue;
        if (familyCapabilities.contains(capability))
            continue;
        if (capability == QStringLiteral("mixed-language"))
            continue;
        return false;
    }
    return true;
}

QString ExampleManager::resolveLocalPath(const QString &path) const
{
    if (path.isEmpty())
        return QString();

    QFileInfo direct(path);
    if (direct.isAbsolute() && direct.exists())
        return direct.absoluteFilePath();

    const QString src = sourceDir();
    if (!src.isEmpty()) {
        QFileInfo sourceCandidate(QDir(src).absoluteFilePath(path));
        if (sourceCandidate.exists())
            return sourceCandidate.absoluteFilePath();
    }

    QFileInfo cwdCandidate(QDir::current().absoluteFilePath(path));
    if (cwdCandidate.exists())
        return cwdCandidate.absoluteFilePath();

    QFileInfo appCandidate(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(path));
    if (appCandidate.exists())
        return appCandidate.absoluteFilePath();

    return QString();
}

} // namespace LAStudio
