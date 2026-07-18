#include "CapabilitySettingsSchema.h"

namespace LAStudio::CapabilitySettingsSchema {

QVariantList merge(const QVariantMap &familyConfig,
                   const QString &capabilityId,
                   const QVariantList &runtimeSchema)
{
    if (familyConfig.isEmpty()) return runtimeSchema;

    const QVariantMap studio = familyConfig.value(QStringLiteral("studio")).toMap();
    const QVariantMap studioConfig = studio.value(capabilityId).toMap();
    const QVariantList parameterIds = studioConfig.value(QStringLiteral("parameters")).toList();
    if (studioConfig.isEmpty() || parameterIds.isEmpty()) return runtimeSchema;

    const QVariantMap definitions = familyConfig.value(QStringLiteral("parameterDefinitions")).toMap();
    QVariantList schema;

    for (const QVariant &idValue : parameterIds) {
        const QString id = idValue.toString();
        QVariantMap merged;

        for (const QVariant &runtimeValue : runtimeSchema) {
            const QVariantMap runtimeItem = runtimeValue.toMap();
            if (runtimeItem.value(QStringLiteral("id")).toString() == id) {
                merged = runtimeItem;
                break;
            }
        }

        const QVariantMap definition = definitions.value(id).toMap();
        for (auto it = definition.cbegin(); it != definition.cend(); ++it)
            merged.insert(it.key(), it.value());

        if (merged.isEmpty()) continue;

        // Some catalog entries use options while the shared QML control uses
        // choices. Keep both formats accepted without duplicating UI logic.
        if (merged.value(QStringLiteral("type")).toString() == QStringLiteral("choice")
            && !merged.contains(QStringLiteral("choices"))
            && merged.contains(QStringLiteral("options"))) {
            merged.insert(QStringLiteral("choices"), merged.value(QStringLiteral("options")));
        }

        // Voice labels may be enriched by catalog speaker metadata while the
        // actual values/choices come from the loaded runtime.
        if (id == QStringLiteral("voice") && familyConfig.contains(QStringLiteral("speakersMetadata"))) {
            const QVariantList metadata = familyConfig.value(QStringLiteral("speakersMetadata")).toList();
            QVariantList choices;
            for (const QVariant &choiceValue : merged.value(QStringLiteral("choices")).toList()) {
                QVariantMap choice = choiceValue.toMap();
                const QString value = choice.value(QStringLiteral("value")).toString();
                for (const QVariant &metadataValue : metadata) {
                    const QVariantMap item = metadataValue.toMap();
                    if (item.value(QStringLiteral("name")).toString().compare(value, Qt::CaseInsensitive) == 0) {
                        choice.insert(QStringLiteral("text"), item.value(QStringLiteral("displayName"), choice.value(QStringLiteral("text"))));
                        choice.insert(QStringLiteral("detail"), item.value(QStringLiteral("language")));
                        break;
                    }
                }
                choices.append(choice);
            }
            if (!choices.isEmpty()) merged.insert(QStringLiteral("choices"), choices);
        }

        merged.insert(QStringLiteral("id"), id);
        schema.append(merged);
    }

    return schema.isEmpty() ? runtimeSchema : schema;
}

} // namespace LAStudio::CapabilitySettingsSchema
