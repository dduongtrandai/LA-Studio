#include "EnglishTransliterator.h"

#include <QRegularExpression>

namespace vietnorm::detail {

bool isVietnameseWord(const QString &word)
{
    const QString w = word.trimmed().toLower();
    if (w.isEmpty()) return false;
    static const QRegularExpression accent(QStringLiteral("[àáảãạăằắẳẵặâầấẩẫậèéẻẽẹêềếểễệìíỉĩịòóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵđ]"));
    if (accent.match(w).hasMatch()) return true;
    if (w.contains(QRegularExpression(QStringLiteral("[fwzj]")))) return false;
    static const QRegularExpression syllable(QStringLiteral("^([^ueoaiy]*)([ueoaiy]+)([^ueoaiy]*)$"));
    const auto match = syllable.match(w);
    if (!match.hasMatch()) return false;
    static const QSet<QString> onsets = {QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d"), QStringLiteral("đ"), QStringLiteral("g"), QStringLiteral("h"), QStringLiteral("k"), QStringLiteral("l"), QStringLiteral("m"), QStringLiteral("n"), QStringLiteral("p"), QStringLiteral("q"), QStringLiteral("r"), QStringLiteral("s"), QStringLiteral("t"), QStringLiteral("v"), QStringLiteral("x"), QStringLiteral("ch"), QStringLiteral("gh"), QStringLiteral("gi"), QStringLiteral("kh"), QStringLiteral("ng"), QStringLiteral("nh"), QStringLiteral("ph"), QStringLiteral("qu"), QStringLiteral("th"), QStringLiteral("tr")};
    static const QSet<QString> endings = {QStringLiteral("p"), QStringLiteral("t"), QStringLiteral("c"), QStringLiteral("m"), QStringLiteral("n"), QStringLiteral("ng"), QStringLiteral("ch"), QStringLiteral("nh")};
    const QString onset = match.captured(1), vowel = match.captured(2), ending = match.captured(3);
    if ((!onset.isEmpty() && !onsets.contains(onset)) || (!ending.isEmpty() && !endings.contains(ending))) return false;
    if (vowel.contains(QRegularExpression(QStringLiteral("ee|oo|ea|ae|ie"))) && vowel != QStringLiteral("oa") && vowel != QStringLiteral("oe") && vowel != QStringLiteral("ua") && vowel != QStringLiteral("uy")) return false;
    return true;
}

QString transliterateWord(const QString &word)
{
    if (word.isEmpty() || isVietnameseWord(word)) return word;
    QString w = word.toLower();
    static const QList<QPair<QString, QString>> rules = {
        {QStringLiteral("tion"), QStringLiteral("ân")}, {QStringLiteral("sion"), QStringLiteral("ân")},
        {QStringLiteral("ture"), QStringLiteral("chờ")}, {QStringLiteral("ough"), QStringLiteral("ao")},
        {QStringLiteral("ight"), QStringLiteral("ai")}, {QStringLiteral("eigh"), QStringLiteral("ây")},
        {QStringLiteral("ph"), QStringLiteral("ph")}, {QStringLiteral("sh"), QStringLiteral("s")},
        {QStringLiteral("ch"), QStringLiteral("ch")}, {QStringLiteral("th"), QStringLiteral("th")},
        {QStringLiteral("ck"), QStringLiteral("c")}, {QStringLiteral("qu"), QStringLiteral("q")},
        {QStringLiteral("oo"), QStringLiteral("u")}, {QStringLiteral("ee"), QStringLiteral("i")},
        {QStringLiteral("a"), QStringLiteral("a")}, {QStringLiteral("e"), QStringLiteral("e")},
        {QStringLiteral("i"), QStringLiteral("i")}, {QStringLiteral("o"), QStringLiteral("ô")},
        {QStringLiteral("u"), QStringLiteral("u")}
    };
    for (const auto &rule : rules) w.replace(rule.first, rule.second);
    w.replace(QRegularExpression(QStringLiteral("[^a-zà-ỹđ-]")), QString());
    if (w.isEmpty()) return word;
    if (word.at(0).isUpper()) w[0] = w.at(0).toUpper();
    return w;
}

} // namespace vietnorm::detail
