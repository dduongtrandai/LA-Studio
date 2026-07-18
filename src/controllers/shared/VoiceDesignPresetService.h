#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqml.h>

namespace LAStudio {

class VoiceDesignPresetService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("VoiceDesignPresetService is managed by AppController")

public:
    explicit VoiceDesignPresetService(QObject *parent = nullptr);
    ~VoiceDesignPresetService() override = default;

    Q_INVOKABLE QVariantList presetsForFamily(const QString &familyId);
    Q_INVOKABLE bool addPreset(const QString &familyId, const QString &name, const QString &description);
    Q_INVOKABLE bool updatePreset(const QString &id, const QString &name, const QString &description);
    Q_INVOKABLE bool deletePreset(const QString &id);

signals:
    void presetsChanged(const QString &familyId);
    void errorOccurred(const QString &msg);

private:
    QString presetsFilePath() const;
    QVariantList loadAllPresets() const;
    bool saveAllPresets(const QVariantList &presets);
};

} // namespace LAStudio
