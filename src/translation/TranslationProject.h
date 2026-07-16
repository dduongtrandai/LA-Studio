#pragma once

#include <QString>
#include <QVariantList>

namespace LAStudio {

class TranslationProject
{
public:
    static constexpr int CurrentSchemaVersion = 1;
    QString projectPath;
    QString sourcePath;
    QString sourceFormat = QStringLiteral("text");
    QString sourceLanguage = QStringLiteral("en");
    QString targetLanguage = QStringLiteral("vi");
    QVariantList segments;

    bool save(QString *error = nullptr) const;
    static bool load(const QString &path, TranslationProject &project, QString *error = nullptr);
    static bool importText(const QString &text, TranslationProject &project, QString *error = nullptr);
    static bool importSubtitle(const QString &text, const QString &format, TranslationProject &project, QString *error = nullptr);
    QString exportText() const;
    QString exportSubtitle(QString *error = nullptr) const;
};

} // namespace LAStudio
