#include "dubbing/EspeakNgPhonemizer.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>

namespace LAStudio {
namespace {

using InitializeFn = int (*)(int, int, const char *, int);
using TerminateFn = int (*)();
using SetVoiceFn = int (*)(const char *);
using TextToPhonemesFn = const char *(*)(const void **, int, int);

struct Api {
    QLibrary library;
    InitializeFn initialize = nullptr;
    TerminateFn terminate = nullptr;
    SetVoiceFn setVoice = nullptr;
    TextToPhonemesFn textToPhonemes = nullptr;
    bool initialized = false;
    QString error;
};

QMutex &apiMutex()
{
    static QMutex mutex;
    return mutex;
}

QString findDependencyFile(const QStringList &names)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList roots{appDir, QDir(appDir).absoluteFilePath(QStringLiteral("espeak-ng"))};
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("espeak-ng"));
    if (!executable.isEmpty()) roots.append(QFileInfo(executable).absolutePath());
    for (const QString &root : roots) {
        for (const QString &name : names) {
            const QString path = QDir(root).absoluteFilePath(name);
            if (QFileInfo::exists(path)) return path;
        }
    }

    // The Windows MSI is extracted with an extra "Program Files/eSpeak NG"
    // level. Search only below the application directory, once, on demand.
    QDirIterator it(appDir, names, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) return it.next();
    return {};
}

QString findDataRoot()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList roots{appDir, QDir(appDir).absoluteFilePath(QStringLiteral("espeak-ng"))};
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("espeak-ng"));
    if (!executable.isEmpty()) roots.append(QFileInfo(executable).absolutePath());
    for (const QString &root : roots) {
        if (QDir(root).exists(QStringLiteral("espeak-ng-data"))) return root;
        if (QDir(root).exists(QStringLiteral("data"))
            && QDir(root + QStringLiteral("/data")).exists(QStringLiteral("voices")))
            return root;
    }
    QDirIterator it(appDir, QStringList{QStringLiteral("espeak-ng-data")},
                    QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    if (it.hasNext()) return QFileInfo(it.next()).absolutePath();
    return {};
}

Api &api()
{
    static Api value;
    if (value.initialized || !value.error.isEmpty()) return value;

    const QString dll = findDependencyFile({QStringLiteral("libespeak-ng.dll"),
                                             QStringLiteral("espeak-ng.dll"),
                                             QStringLiteral("espeak.dll")});
    value.library.setFileName(dll.isEmpty() ? QStringLiteral("libespeak-ng") : dll);
    if (!value.library.load()) {
        value.error = QStringLiteral("libespeak-ng is unavailable: %1").arg(value.library.errorString());
        Logger::error(QStringLiteral("EspeakNgPhonemizer"), value.error);
        return value;
    }

    value.initialize = reinterpret_cast<InitializeFn>(value.library.resolve("espeak_Initialize"));
    value.terminate = reinterpret_cast<TerminateFn>(value.library.resolve("espeak_Terminate"));
    value.setVoice = reinterpret_cast<SetVoiceFn>(value.library.resolve("espeak_SetVoiceByName"));
    value.textToPhonemes = reinterpret_cast<TextToPhonemesFn>(
        value.library.resolve("espeak_TextToPhonemes"));
    if (!value.initialize || !value.terminate || !value.setVoice || !value.textToPhonemes) {
        value.error = QStringLiteral("libespeak-ng is missing the phonemizer C API.");
        Logger::error(QStringLiteral("EspeakNgPhonemizer"), value.error);
        value.library.unload();
        return value;
    }

    const QByteArray dataRoot = findDataRoot().toUtf8();
    const int sampleRate = value.initialize(0, 0, dataRoot.isEmpty() ? nullptr : dataRoot.constData(), 0);
    if (sampleRate < 0) {
        value.error = QStringLiteral("libespeak-ng failed to initialize its data files.");
        Logger::error(QStringLiteral("EspeakNgPhonemizer"),
                      QStringLiteral("%1 library=%2 dataRoot=%3")
                          .arg(value.error, value.library.fileName(),
                               QString::fromUtf8(dataRoot)));
        value.library.unload();
        return value;
    }
    value.initialized = true;
    Logger::info(QStringLiteral("EspeakNgPhonemizer"),
                 QStringLiteral("Initialized library=%1 dataRoot=%2 sampleRate=%3")
                     .arg(value.library.fileName(), QString::fromUtf8(dataRoot))
                     .arg(sampleRate));
    return value;
}

QString voiceForLanguage(QString language)
{
    language = language.trimmed().toLower();
    if (language == QStringLiteral("vi")) return QStringLiteral("vi");
    if (language == QStringLiteral("en")) return QStringLiteral("en");
    return language;
}

} // namespace

bool EspeakNgPhonemizer::phonemize(const QString &text, const QString &language,
                                   QString &phonemes, QString *error)
{
    QMutexLocker locker(&apiMutex());
    Api &e = api();
    if (!e.initialized) {
        if (error) *error = e.error;
        return false;
    }
    const QByteArray voice = voiceForLanguage(language).toUtf8();
    if (e.setVoice(voice.constData()) != 0) {
        if (error) *error = QStringLiteral("eSpeak NG does not support language '%1'.").arg(language);
        Logger::error(QStringLiteral("EspeakNgPhonemizer"),
                      QStringLiteral("Voice selection failed language=%1 voice=%2")
                          .arg(language, QString::fromUtf8(voice)));
        return false;
    }

    const QByteArray input = text.toUtf8();
    const void *cursor = input.constData();
    QStringList parts;
    // phonememode bit 8-23 selects the separator. A space makes tokenization
    // deterministic, including for multi-character phonemes.
    constexpr int separatorSpace = (int(' ') << 8);
    while (cursor) {
        const char *part = e.textToPhonemes(&cursor, 1, separatorSpace);
        if (part && *part) parts.append(QString::fromUtf8(part));
    }
    phonemes = parts.join(QLatin1Char(' ')).simplified();
    Logger::debug(QStringLiteral("EspeakNgPhonemizer"),
                  QStringLiteral("Phonemized language=%1 inputChars=%2 rawTokens=%3")
                      .arg(language).arg(text.size())
                      .arg(phonemes.split(QRegularExpression(QStringLiteral("\\s+")),
                                           Qt::SkipEmptyParts).size()));
    return true;
}

int EspeakNgPhonemizer::count(const QString &text, const QString &language,
                              bool *usedLibrary, QString *error)
{
    QString result;
    QString localError;
    QString *errorTarget = error ? error : &localError;
    const bool ok = phonemize(text, language, result, errorTarget);
    if (usedLibrary) *usedLibrary = ok;
    if (!ok) {
        Logger::error(QStringLiteral("EspeakNgPhonemizer"),
                      QStringLiteral("Count failed language=%1 inputChars=%2 error=%3")
                          .arg(language).arg(text.size()).arg(*errorTarget));
        return -1;
    }
    if (result.isEmpty()) return 0;

    int count = 0;
    const QStringList tokens = result.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        // eSpeak may emit stress markers and punctuation-like separators;
        // count actual phone tokens only.
        // `_` and `_|` are eSpeak word/clause boundary markers, not phones.
        if (token.startsWith(QLatin1Char('_')) || token == QStringLiteral("|")) continue;
        ++count;
    }
    Logger::debug(QStringLiteral("EspeakNgPhonemizer"),
                  QStringLiteral("Counted language=%1 inputChars=%2 phonemes=%3")
                      .arg(language).arg(text.size()).arg(count));
    return count;
}

} // namespace LAStudio
