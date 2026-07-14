#include "TtsRequestValidator.h"

#include <QMetaType>

#include <cmath>

namespace LAStudio {
namespace {

bool permitsInput(const QVariantMap &studioConfig, const QString &input)
{
    const QVariantList inputs = studioConfig.value(QStringLiteral("inputs")).toList();
    for (const QVariant &inputValue : inputs) {
        if (inputValue.toString() == input)
            return true;
    }
    return false;
}

bool normalizeValue(const QVariantMap &parameter,
                    const QVariant &rawValue,
                    QVariant &normalizedValue,
                    QString &error)
{
    const QString id = parameter.value(QStringLiteral("id")).toString();
    const QString type = parameter.value(QStringLiteral("type"), QStringLiteral("string")).toString();

    if (type == QStringLiteral("choice")) {
        QVariantList choices = parameter.value(QStringLiteral("choices")).toList();
        if (choices.isEmpty())
            choices = parameter.value(QStringLiteral("options")).toList();
        for (const QVariant &choiceValue : choices) {
            const QVariantMap choice = choiceValue.toMap();
            if (choice.value(QStringLiteral("value")).toString() == rawValue.toString()) {
                normalizedValue = choice.value(QStringLiteral("value"));
                return true;
            }
        }
        error = QStringLiteral("Invalid value for TTS setting '%1': value is not an active choice.").arg(id);
        return false;
    }

    if (type == QStringLiteral("bool")) {
        if (rawValue.metaType().id() != QMetaType::Bool) {
            error = QStringLiteral("Invalid type for TTS setting '%1': expected bool.").arg(id);
            return false;
        }
        normalizedValue = rawValue.toBool();
        return true;
    }

    if (type == QStringLiteral("int")) {
        bool ok = false;
        const double number = rawValue.toDouble(&ok);
        if (!ok || !std::isfinite(number) || std::floor(number) != number) {
            error = QStringLiteral("Invalid type for TTS setting '%1': expected int.").arg(id);
            return false;
        }
        if ((parameter.contains(QStringLiteral("min")) && number < parameter.value(QStringLiteral("min")).toDouble()) ||
            (parameter.contains(QStringLiteral("max")) && number > parameter.value(QStringLiteral("max")).toDouble())) {
            error = QStringLiteral("Invalid value for TTS setting '%1': value is outside the allowed range.").arg(id);
            return false;
        }
        normalizedValue = static_cast<qint64>(number);
        return true;
    }

    if (type == QStringLiteral("float")) {
        bool ok = false;
        const double number = rawValue.toDouble(&ok);
        if (!ok || !std::isfinite(number)) {
            error = QStringLiteral("Invalid type for TTS setting '%1': expected float.").arg(id);
            return false;
        }
        if ((parameter.contains(QStringLiteral("min")) && number < parameter.value(QStringLiteral("min")).toDouble()) ||
            (parameter.contains(QStringLiteral("max")) && number > parameter.value(QStringLiteral("max")).toDouble())) {
            error = QStringLiteral("Invalid value for TTS setting '%1': value is outside the allowed range.").arg(id);
            return false;
        }
        normalizedValue = number;
        return true;
    }

    if (type == QStringLiteral("string")) {
        if (rawValue.metaType().id() != QMetaType::QString) {
            error = QStringLiteral("Invalid type for TTS setting '%1': expected string.").arg(id);
            return false;
        }
        normalizedValue = rawValue.toString();
        return true;
    }

    error = QStringLiteral("Unsupported TTS schema type '%1' for setting '%2'.").arg(type, id);
    return false;
}

} // namespace

bool TtsRequestValidator::normalize(const QVariantList &schema,
                                    const QVariantMap &studioConfig,
                                    const QVariantMap &rawSettings,
                                    QVariantMap &normalizedSettings,
                                    QString &error)
{
    normalizedSettings.clear();
    error.clear();

    const QVariantList requiredInputs = studioConfig.value(QStringLiteral("requiredInputs")).toList();
    for (const QVariant &reqInput : requiredInputs) {
        const QString name = reqInput.toString();
        if (name == QStringLiteral("instruct")) {
            if (!rawSettings.contains(QStringLiteral("instruct")) || rawSettings.value(QStringLiteral("instruct")).toString().trimmed().isEmpty()) {
                error = QStringLiteral("Missing required input: instruct");
                return false;
            }
        } else if (name == QStringLiteral("language")) {
            if (!rawSettings.contains(QStringLiteral("lang")) || rawSettings.value(QStringLiteral("lang")).toString().trimmed().isEmpty()) {
                error = QStringLiteral("Missing required input: language");
                return false;
            }
        }
    }

    QVariantMap parameters;
    for (const QVariant &parameterValue : schema) {
        const QVariantMap parameter = parameterValue.toMap();
        const QString id = parameter.value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
            parameters.insert(id, parameter);
    }

    const bool acceptsLanguage = permitsInput(studioConfig, QStringLiteral("language"));
    const bool acceptsInstruct = permitsInput(studioConfig, QStringLiteral("instruct"));

    for (auto it = rawSettings.constBegin(); it != rawSettings.constEnd(); ++it) {
        if (parameters.contains(it.key()))
            continue;
        if (it.key() == QStringLiteral("lang") && acceptsLanguage)
            continue;
        if (it.key() == QStringLiteral("instruct") && acceptsInstruct)
            continue;

        error = QStringLiteral("Unsupported TTS setting for the active model: '%1'.").arg(it.key());
        return false;
    }

    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        const QVariantMap parameter = it.value().toMap();
        QVariant value;
        if (rawSettings.contains(it.key())) {
            value = rawSettings.value(it.key());
        } else if (parameter.contains(QStringLiteral("default"))) {
            value = parameter.value(QStringLiteral("default"));
        } else {
            continue;
        }

        QVariant normalizedValue;
        if (!normalizeValue(parameter, value, normalizedValue, error))
            return false;
        normalizedSettings.insert(it.key(), normalizedValue);
    }

    if (rawSettings.contains(QStringLiteral("lang"))) {
        if (rawSettings.value(QStringLiteral("lang")).metaType().id() != QMetaType::QString) {
            error = QStringLiteral("Invalid type for TTS input 'lang': expected string.");
            return false;
        }
        normalizedSettings.insert(QStringLiteral("lang"), rawSettings.value(QStringLiteral("lang")).toString());
    }

    if (rawSettings.contains(QStringLiteral("instruct"))) {
        if (rawSettings.value(QStringLiteral("instruct")).metaType().id() != QMetaType::QString) {
            error = QStringLiteral("Invalid type for TTS input 'instruct': expected string.");
            return false;
        }
        normalizedSettings.insert(QStringLiteral("instruct"), rawSettings.value(QStringLiteral("instruct")).toString());
    }

    return true;
}

} // namespace LAStudio
