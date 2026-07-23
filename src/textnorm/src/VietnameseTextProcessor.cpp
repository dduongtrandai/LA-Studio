#include "VietnameseTextProcessor.h"

#include <QRegularExpression>
#include <QStringList>
#include <functional>

namespace vietnorm::detail {
namespace {

using Replacer = std::function<QString(const QRegularExpressionMatch &)>;

QString replaceMatches(const QString &input, const QRegularExpression &pattern,
                       const Replacer &replacer)
{
    QString output;
    output.reserve(input.size());
    int cursor = 0;
    auto iterator = pattern.globalMatch(input);
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        const int start = match.capturedStart();
        const int end = match.capturedEnd();
        if (start < cursor)
            continue;
        output += input.mid(cursor, start - cursor);
        output += replacer(match);
        cursor = end;
    }
    output += input.mid(cursor);
    return output;
}

QString digitsToWords(const QString &digits)
{
    static const QStringList names = {
        QStringLiteral("không"), QStringLiteral("một"), QStringLiteral("hai"),
        QStringLiteral("ba"), QStringLiteral("bốn"), QStringLiteral("năm"),
        QStringLiteral("sáu"), QStringLiteral("bảy"), QStringLiteral("tám"),
        QStringLiteral("chín")};
    QStringList result;
    for (const QChar ch : digits) {
        if (ch.isDigit())
            result.append(names.value(ch.digitValue(), QString(ch)));
    }
    return result.join(QLatin1Char(' '));
}

QString readUnderThousand(int value, bool forceHundreds)
{
    static const QStringList d = {
        QStringLiteral("không"), QStringLiteral("một"), QStringLiteral("hai"),
        QStringLiteral("ba"), QStringLiteral("bốn"), QStringLiteral("năm"),
        QStringLiteral("sáu"), QStringLiteral("bảy"), QStringLiteral("tám"),
        QStringLiteral("chín")};
    if (value == 0)
        return QString();

    QStringList result;
    const int hundreds = value / 100;
    const int remainder = value % 100;
    if (hundreds > 0 || forceHundreds)
        result << d.value(hundreds) << QStringLiteral("trăm");

    if (remainder == 0)
        return result.join(QLatin1Char(' '));
    if (remainder < 10) {
        if (hundreds > 0 || forceHundreds)
            result << QStringLiteral("lẻ");
        result << d.value(remainder);
        return result.join(QLatin1Char(' '));
    }

    const int tens = remainder / 10;
    const int units = remainder % 10;
    result << (tens == 1 ? QStringLiteral("mười") : d.value(tens) + QStringLiteral(" mươi"));
    if (units == 1 && tens > 1)
        result << QStringLiteral("mốt");
    else if (units == 4 && tens > 1)
        result << QStringLiteral("tư");
    else if (units == 5)
        result << QStringLiteral("lăm");
    else if (units > 0)
        result << d.value(units);
    return result.join(QLatin1Char(' '));
}

} // namespace

QString VietnameseTextProcessor::processGroup(int value, bool hundreds) const
{
    return readUnderThousand(value, hundreds);
}

QString VietnameseTextProcessor::numberToWords(const QString &rawDigits) const
{
    QString digits = rawDigits.trimmed();
    if (digits.isEmpty())
        return QString();
    bool negative = digits.startsWith(QLatin1Char('-'));
    if (negative)
        digits.remove(0, 1);
    digits.remove(QRegularExpression(QStringLiteral("^0+")));
    if (digits.isEmpty())
        digits = QStringLiteral("0");
    if (digits == QStringLiteral("0"))
        return negative ? QStringLiteral("âm không") : QStringLiteral("không");

    bool ok = false;
    const qulonglong number = digits.toULongLong(&ok);
    if (!ok || number > 999999999999ULL)
        return (negative ? QStringLiteral("âm ") : QString()) + digitsToWords(digits);

    const int billions = static_cast<int>(number / 1000000000ULL);
    const int millions = static_cast<int>((number / 1000000ULL) % 1000ULL);
    const int thousands = static_cast<int>((number / 1000ULL) % 1000ULL);
    const int remainder = static_cast<int>(number % 1000ULL);
    QStringList parts;
    if (billions > 0)
        parts << readUnderThousand(billions, false) << QStringLiteral("tỷ");
    if (millions > 0)
        parts << readUnderThousand(millions, !parts.isEmpty() && millions < 100) << QStringLiteral("triệu");
    else if (!parts.isEmpty() && (thousands > 0 || remainder > 0))
        parts << QStringLiteral("không triệu");
    if (thousands > 0)
        parts << readUnderThousand(thousands, !parts.isEmpty() && thousands < 100) << QStringLiteral("nghìn");
    else if (!parts.isEmpty() && remainder > 0)
        parts << QStringLiteral("không nghìn");
    if (remainder > 0)
        parts << readUnderThousand(remainder, !parts.isEmpty() && remainder < 100);
    return (negative ? QStringLiteral("âm ") : QString()) + parts.join(QLatin1Char(' '));
}

QString VietnameseTextProcessor::process(const QString &input) const
{
    if (input.isEmpty())
        return QString();
    QString text = input.normalized(QString::NormalizationForm_C);

    text.replace(QLatin1Char('&'), QStringLiteral(" và "));
    text.replace(QLatin1Char('@'), QStringLiteral(" a còng "));
    text.replace(QLatin1Char('#'), QStringLiteral(" thăng "));
    text.remove(QRegularExpression(QStringLiteral("https?://\\S+|www\\.\\S+|\\S+@\\S+\\.\\S+")));
    text.replace(QRegularExpression(QStringLiteral("[\\*~`^]")), QString());
    text.replace(QLatin1Char('_'), QLatin1Char(' '));
    text.replace(QRegularExpression(QStringLiteral("[\\\"“”„‟]")), QStringLiteral("\""));
    text.replace(QRegularExpression(QStringLiteral("['‘’‚‛]")), QStringLiteral("'"));
    text.replace(QRegularExpression(QStringLiteral("[–—−]")), QStringLiteral("-"));
    text.replace(QRegularExpression(QStringLiteral("\\.{3,}")), QStringLiteral("..."));
    text.replace(QChar(0x2026), QStringLiteral("..."));

    text.remove(QRegularExpression(QStringLiteral("[\\x{1F600}-\\x{1F64F}\\x{1F300}-\\x{1F5FF}\\x{1F680}-\\x{1F6FF}\\x{1F1E0}-\\x{1F1FF}\\x{2600}-\\x{27BF}\\x{1F900}-\\x{1F9FF}\\x{FE0F}\\x{200D}]")));
    text.replace(QRegularExpression(QStringLiteral("[\\\\()¯]")), QString());
    text.replace(QRegularExpression(QStringLiteral("(?<!\\d)-(?!\\d)")), QStringLiteral(" "));
    text.remove(QRegularExpression(QStringLiteral("[^\\x{0000}-\\x{024F}\\x{1E00}-\\x{1EFF}\\d\\s.,!?%:/+\\-]")));

    const QRegularExpression addressKeyword(QStringLiteral("(số|nhà|đường|hẻm|ngõ|ngách|kiệt|phố)\\s+(\\d+(?:/\\d+)+)"), QRegularExpression::CaseInsensitiveOption);
    text = replaceMatches(text, addressKeyword, [this](const auto &m) {
        const auto parts = m.captured(2).split(QLatin1Char('/'));
        QStringList words;
        for (const auto &part : parts) words << numberToWords(part);
        return m.captured(1) + QLatin1Char(' ') + words.join(QStringLiteral(" trên "));
    });
    const QRegularExpression address3(QStringLiteral("\\b(\\d+)/(\\d+)/(\\d{1,3})\\b"));
    text = replaceMatches(text, address3, [this](const auto &m) {
        return numberToWords(m.captured(1)) + QStringLiteral(" trên ") + numberToWords(m.captured(2)) + QStringLiteral(" trên ") + numberToWords(m.captured(3));
    });
    const QRegularExpression address2(QStringLiteral("\\b(\\d{3,})/(\\d+)\\b"));
    text = replaceMatches(text, address2, [this](const auto &m) {
        return numberToWords(m.captured(1)) + QStringLiteral(" trên ") + numberToWords(m.captured(2));
    });

    const QRegularExpression yearRange(QStringLiteral("(\\d{4})\\s*[-–—]\\s*(\\d{4})"));
    text = replaceMatches(text, yearRange, [this](const auto &m) {
        return numberToWords(m.captured(1)) + QStringLiteral(" đến ") + numberToWords(m.captured(2));
    });
    const QRegularExpression percentRange(QStringLiteral("(\\d+)\\s*[-–—]\\s*(\\d+)\\s*%"));
    text = replaceMatches(text, percentRange, [this](const auto &m) {
        return numberToWords(m.captured(1)) + QStringLiteral(" đến ") + numberToWords(m.captured(2)) + QStringLiteral(" phần trăm");
    });

    const QRegularExpression dateRange(QStringLiteral("(\\d{1,2})\\s*[-–—]\\s*(\\d{1,2})\\s*[/-](\\d{1,2})(?:\\s*[/-](\\d{4}))?"));
    text = replaceMatches(text, dateRange, [this](const auto &m) {
        const int d1 = m.captured(1).toInt(), d2 = m.captured(2).toInt(), month = m.captured(3).toInt();
        if (d1 < 1 || d1 > 31 || d2 < 1 || d2 > 31 || month < 1 || month > 12) return m.captured(0);
        QString result = numberToWords(m.captured(1)) + QStringLiteral(" đến ") + numberToWords(m.captured(2)) + QStringLiteral(" tháng ") + numberToWords(m.captured(3));
        if (!m.captured(4).isEmpty()) result += QStringLiteral(" năm ") + numberToWords(m.captured(4));
        return result;
    });
    const QRegularExpression fullDate(QStringLiteral("(\\d{1,2})[/-](\\d{1,2})[/-](\\d{4})"));
    text = replaceMatches(text, fullDate, [this](const auto &m) {
        const int day = m.captured(1).toInt(), month = m.captured(2).toInt(), year = m.captured(3).toInt();
        if (day < 1 || day > 31 || month < 1 || month > 12 || year < 1000 || year > 9999) return m.captured(0);
        return QStringLiteral("ngày ") + numberToWords(m.captured(1)) + QStringLiteral(" tháng ") + numberToWords(m.captured(2)) + QStringLiteral(" năm ") + numberToWords(m.captured(3));
    });
    const QRegularExpression dayMonth(QStringLiteral("(\\d{1,2})\\s*[/-]\\s*(\\d{1,2})(?![/-]\\d)(?!\\d+\\s*%)"));
    text = replaceMatches(text, dayMonth, [this](const auto &m) {
        const int day = m.captured(1).toInt(), month = m.captured(2).toInt();
        return (day >= 1 && day <= 31 && month >= 1 && month <= 12)
            ? numberToWords(m.captured(1)) + QStringLiteral(" tháng ") + numberToWords(m.captured(2)) : m.captured(0);
    });
    const QRegularExpression monthOnly(QStringLiteral("tháng\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    text = replaceMatches(text, monthOnly, [this](const auto &m) {
        const int month = m.captured(1).toInt();
        return (month >= 1 && month <= 12) ? QStringLiteral("tháng ") + numberToWords(m.captured(1)) : m.captured(0);
    });

    const QRegularExpression hms(QStringLiteral("(\\d{1,2}):(\\d{2})(?::(\\d{2}))?"));
    text = replaceMatches(text, hms, [this](const auto &m) {
        QString out = numberToWords(m.captured(1)) + QStringLiteral(" giờ ") + numberToWords(m.captured(2)) + QStringLiteral(" phút");
        if (!m.captured(3).isEmpty()) out += QStringLiteral(" ") + numberToWords(m.captured(3)) + QStringLiteral(" giây");
        return out;
    });
    const QRegularExpression ordinal(QStringLiteral("(thứ|lần|bước|phần|chương|tập|số)\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    text = replaceMatches(text, ordinal, [this](const auto &m) { return m.captured(1) + QLatin1Char(' ') + numberToWords(m.captured(2)); });
    const QRegularExpression thousands(QStringLiteral("(\\d{1,3}(?:\\.\\d{3})+)(?=\\s|$|[^\\d.,])"));
    text = replaceMatches(text, thousands, [](const auto &m) {
        QString value = m.captured(1);
        value.remove(QLatin1Char('.'));
        return value;
    });
    const QRegularExpression currency(QStringLiteral("(\\d+(?:[.,]\\d+)?)\\s*(?:đồng|VND|vnđ|đ|USD|\\$)|\\$\\s*(\\d+(?:,\\d+)?)"), QRegularExpression::CaseInsensitiveOption);
    text = replaceMatches(text, currency, [this](const auto &m) {
        QString amount = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        const bool usd = m.captured(0).contains(QLatin1Char('$'), Qt::CaseInsensitive) || m.captured(0).contains(QStringLiteral("USD"), Qt::CaseInsensitive);
        amount.remove(QLatin1Char('.')); amount.remove(QLatin1Char(','));
        return numberToWords(amount) + (usd ? QStringLiteral(" đô la") : QStringLiteral(" đồng"));
    });
    const QRegularExpression percentage(QStringLiteral("(\\d+),(\\d+)\\s*%|(?<![\\d,])(\\d+)\\s*%"));
    text = replaceMatches(text, percentage, [this](const auto &m) {
        if (!m.captured(1).isEmpty()) return numberToWords(m.captured(1)) + QStringLiteral(" phẩy ") + numberToWords(m.captured(2)) + QStringLiteral(" phần trăm");
        return numberToWords(m.captured(3)) + QStringLiteral(" phần trăm");
    });
    const QRegularExpression decimal(QStringLiteral("(\\d+),(\\d+)(?=\\s|$|[^\\d,])"));
    text = replaceMatches(text, decimal, [this](const auto &m) { return numberToWords(m.captured(1)) + QStringLiteral(" phẩy ") + numberToWords(m.captured(2)); });
    const QRegularExpression phone(QStringLiteral("(?:\\+84\\d{9,10}|0\\d{9,10})"));
    text = replaceMatches(text, phone, [](const auto &m) { return digitsToWords(m.captured(0).remove(QRegularExpression(QStringLiteral("^\\+84")))); });

    static const QList<QPair<QString, QString>> units = {
        {QStringLiteral("km/h"), QStringLiteral("ki-lô-mét trên giờ")}, {QStringLiteral("m/s"), QStringLiteral("mét trên giây")},
        {QStringLiteral("mm"), QStringLiteral("mi-li-mét")}, {QStringLiteral("cm"), QStringLiteral("xăng-ti-mét")},
        {QStringLiteral("km"), QStringLiteral("ki-lô-mét")}, {QStringLiteral("kg"), QStringLiteral("ki-lô-gam")},
        {QStringLiteral("mg"), QStringLiteral("mi-li-gam")}, {QStringLiteral("ml"), QStringLiteral("mi-li-lít")},
        {QStringLiteral("m2"), QStringLiteral("mét vuông")}, {QStringLiteral("m3"), QStringLiteral("mét khối")},
        {QStringLiteral("m"), QStringLiteral("mét")}, {QStringLiteral("g"), QStringLiteral("gam")}, {QStringLiteral("l"), QStringLiteral("lít")}
    };
    for (const auto &unit : units) {
        const QRegularExpression unitPattern(QStringLiteral("(\\d+)\\s*%1(?![A-Za-zÀ-ỹ])").arg(QRegularExpression::escape(unit.first)), QRegularExpression::CaseInsensitiveOption);
        text = replaceMatches(text, unitPattern, [&unit](const auto &m) { return m.captured(1) + QLatin1Char(' ') + unit.second; });
    }
    const QRegularExpression roman(QStringLiteral("\\b([IVXLC]{2,})\\b"));
    text = replaceMatches(text, roman, [this](const auto &m) {
        int total = 0, previous = 0;
        const QString value = m.captured(1);
        for (int i = value.size() - 1; i >= 0; --i) {
            const int v = value.at(i) == QLatin1Char('I') ? 1 : value.at(i) == QLatin1Char('V') ? 5 : value.at(i) == QLatin1Char('X') ? 10 : value.at(i) == QLatin1Char('L') ? 50 : 100;
            if (v < previous) total -= v; else total += v;
            previous = v;
        }
        return (total > 0 && total < 100) ? numberToWords(QString::number(total)) : m.captured(0);
    });
    const QRegularExpression standalone(QStringLiteral("\\b\\d+\\b"));
    text = replaceMatches(text, standalone, [this](const auto &m) { return numberToWords(m.captured(0)); });
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.trimmed();
}

} // namespace vietnorm::detail
