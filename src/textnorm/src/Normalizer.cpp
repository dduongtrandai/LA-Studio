#include "vietnorm/Normalizer.h"

#include "DictionaryStore.h"
#include "EnglishTransliterator.h"
#include "VietnameseTextProcessor.h"
#include "vietnorm/Version.h"

#include <QRegularExpression>
#include <functional>

namespace vietnorm {
namespace {

QString replaceMatches(const QString &input, const QRegularExpression &pattern,
                       const std::function<QString(const QRegularExpressionMatch &)> &fn)
{
    QString output;
    int cursor = 0;
    auto iterator = pattern.globalMatch(input);
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        output += input.mid(cursor, match.capturedStart() - cursor);
        output += fn(match);
        cursor = match.capturedEnd();
    }
    output += input.mid(cursor);
    return output;
}

} // namespace

class Normalizer::Impl {
public:
    detail::VietnameseTextProcessor processor;
    detail::DictionaryStore dictionaries;
};

Normalizer::Normalizer(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Normalizer::~Normalizer() = default;

std::unique_ptr<Normalizer> Normalizer::create(QString *)
{
    auto impl = std::make_unique<Impl>();
    impl->dictionaries = detail::DictionaryStore::builtIn();
    return std::unique_ptr<Normalizer>(new Normalizer(std::move(impl)));
}

std::unique_ptr<Normalizer> Normalizer::fromDataDirectory(const QString &directory, QString *error)
{
    auto impl = std::make_unique<Impl>();
    if (!detail::DictionaryStore::loadDirectory(directory, impl->dictionaries, error)) return nullptr;
    return std::unique_ptr<Normalizer>(new Normalizer(std::move(impl)));
}

NormalizationResult Normalizer::normalize(QStringView input,
                                          const NormalizationOptions &options) const
{
    NormalizationResult result;
    result.profileId = options.profile == Profile::Compatibility023
        ? QStringLiteral("compatibility-0.2.3") : QStringLiteral("safe-vi-tts-v1");
    result.dataVersion = dataVersion();
    const QString original = input.toString();
    QString text = options.enablePreprocessing ? m_impl->processor.process(original)
                                               : original.normalized(QString::NormalizationForm_C).simplified();
    if (text.isEmpty()) { result.text.clear(); result.changed = !original.isEmpty(); return result; }

    static const QRegularExpression uppercaseCode(QStringLiteral("\\b([A-Z][A-Z0-9]+)\\b"));
    text = replaceMatches(text, uppercaseCode, [this](const QRegularExpressionMatch &match) {
        const QString code = match.captured(1);
        const QString key = code.toLower();
        if (m_impl->dictionaries.acronyms.contains(key)) return m_impl->dictionaries.acronyms.value(key);
        QStringList parts;
        for (const QChar ch : code) parts << (ch.isDigit() ? ch : QString(ch));
        return parts.join(QLatin1Char(' '));
    });
    text = text.toLower();
    static const QRegularExpression words(QStringLiteral("[\\w\\x{00C0}-\\x{1EFF}]+"));
    text = replaceMatches(text, words, [this](const QRegularExpressionMatch &match) {
        const QString key = match.captured(0).toLower();
        return m_impl->dictionaries.foreignWords.value(key, match.captured(0));
    });

    if (options.enableTransliteration || options.profile == Profile::Compatibility023) {
        text = replaceMatches(text, words, [](const QRegularExpressionMatch &match) {
            return detail::transliterateWord(match.captured(0));
        });
    }
    text = text.simplified();
    result.text = text;
    result.changed = text != original;
    return result;
}

} // namespace vietnorm
