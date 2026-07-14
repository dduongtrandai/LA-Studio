#include "VoiceCloningUtils.h"

#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUrl>

namespace {

const QRegularExpression &viPattern()
{
    static const QRegularExpression re(
        QStringLiteral("[àáạảãâầấậẩẫăằắặẳẵèéẹẻẽêềếệểễìíịỉĩòóọỏõôồốộổỗơờớợởỡùúụủũưừứựửữỳýỵỷỹđ]"),
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

const QRegularExpression &zhPattern()
{
    static const QRegularExpression re(QStringLiteral("[\\x{4e00}-\\x{9fa5}]"));
    return re;
}

const QRegularExpression &jaPattern()
{
    static const QRegularExpression re(QStringLiteral("[\\x{3040}-\\x{309f}\\x{30a0}-\\x{30ff}]"));
    return re;
}

const QRegularExpression &koPattern()
{
    static const QRegularExpression re(QStringLiteral("[\\x{ac00}-\\x{d7af}\\x{1100}-\\x{11ff}]"));
    return re;
}

} // namespace

namespace LAStudio {

VoiceCloningUtils::VoiceCloningUtils(QObject *parent)
    : QObject(parent)
{
}

QString VoiceCloningUtils::detectLanguageCode(const QString &text) const
{
    if (text.isEmpty())
        return QStringLiteral("en");

    if (viPattern().match(text).hasMatch())
        return QStringLiteral("vi");
    if (zhPattern().match(text).hasMatch())
        return QStringLiteral("zh");
    if (jaPattern().match(text).hasMatch())
        return QStringLiteral("ja");
    if (koPattern().match(text).hasMatch())
        return QStringLiteral("ko");

    return QStringLiteral("en");
}

QString VoiceCloningUtils::normalizeText(const QString &text) const
{
    return text.normalized(QString::NormalizationForm_C);
}

int VoiceCloningUtils::generateSeed(int upperBound) const
{
    const int bounded = upperBound <= 1 ? 1000000 : upperBound;
    return QRandomGenerator::global()->bounded(bounded);
}

QString VoiceCloningUtils::fileNameFromPath(const QString &path) const
{
    if (path.isEmpty())
        return QString();

    QString cleanPath = path;
    if (cleanPath.startsWith(QStringLiteral("file://"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }

    return QFileInfo(cleanPath).fileName();
}

QVariantMap VoiceCloningUtils::buildCloneSettings(const QString &language,
                                                  const QString &instruction,
                                                  const QString &referenceText,
                                                  bool denoise,
                                                  bool preprocessPrompt,
                                                  const QVariantMap &dynamicSettings,
                                                  bool randomSeed,
                                                  int customSeed) const
{
    QVariantMap settings;
    settings.insert(QStringLiteral("lang"), language);
    settings.insert(QStringLiteral("instruct"), normalizeText(instruction));
    settings.insert(QStringLiteral("ref_text"), normalizeText(referenceText));
    settings.insert(QStringLiteral("denoise"), denoise);
    settings.insert(QStringLiteral("preprocess_prompt"), preprocessPrompt);

    for (auto it = dynamicSettings.constBegin(); it != dynamicSettings.constEnd(); ++it) {
        settings.insert(it.key(), it.value());
    }

    const int seed = randomSeed ? generateSeed() : customSeed;
    // Support both Omnivoice and Vibevoice seed formats
    settings.insert(QStringLiteral("mg_seed"), seed);  // Omnivoice format
    settings.insert(QStringLiteral("seed"), seed);     // Vibevoice format

    return settings;
}

} // namespace LAStudio
