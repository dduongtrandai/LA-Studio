#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqml.h>

namespace LAStudio {

class VoiceClonePresetService : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("VoiceClonePresetService is managed by AppController")

public:
    explicit VoiceClonePresetService(QObject *parent = nullptr);
    ~VoiceClonePresetService() override = default;

    Q_INVOKABLE QVariantList presetsForFamily(const QString &familyId);
    Q_INVOKABLE bool addPreset(const QString &familyId,
                               const QString &name,
                               const QString &audioPath,
                               const QString &referenceText);
    Q_INVOKABLE bool updatePreset(const QString &id,
                                  const QString &name,
                                  const QString &audioPath,
                                  const QString &referenceText);
    Q_INVOKABLE bool deletePreset(const QString &id);

signals:
    void presetsChanged(const QString &familyId);
    void errorOccurred(const QString &msg);

private:
    QString presetsFilePath() const;
    QString audioStorageDir() const;
    QVariantList loadAllPresets() const;
    bool saveAllPresets(const QVariantList &presets);
    QString persistReferenceAudio(const QString &id, const QString &audioPath);
    void removeStoredReferenceAudio(const QString &audioPath);
};

} // namespace LAStudio
