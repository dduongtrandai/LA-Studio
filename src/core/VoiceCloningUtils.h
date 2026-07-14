#pragma once

#include <QObject>
#include <QVariantMap>

#include <QtQml/qqml.h>

namespace LAStudio {

class VoiceCloningUtils : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit VoiceCloningUtils(QObject *parent = nullptr);

    Q_INVOKABLE QString detectLanguageCode(const QString &text) const;
    Q_INVOKABLE QString normalizeText(const QString &text) const;
    Q_INVOKABLE int generateSeed(int upperBound = 1000000) const;
    Q_INVOKABLE QString fileNameFromPath(const QString &path) const;

    Q_INVOKABLE QVariantMap buildCloneSettings(const QString &language,
                                               const QString &instruction,
                                               const QString &referenceText,
                                               bool denoise,
                                               bool preprocessPrompt,
                                               const QVariantMap &dynamicSettings,
                                               bool randomSeed,
                                               int customSeed) const;
};

} // namespace LAStudio
