#pragma once

#include "TtsBackend.h"
#include <atomic>

namespace LAStudio {

class VoxCpm2Backend : public TtsBackend {
public:
    ~VoxCpm2Backend() override;

    bool load(const QVariantMap &config, QString &error, QVariantList &schema) override;
    void unload() override;
    void cancelProcessing() override;
    bool synthesize(const QString &text, float speed, const QVariantMap &settings,
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings,
                    QVector<float> &samples, int &sampleRate, QString &error) override;

private:
    bool synthesizeInternal(const QString &text, const QVariantMap &settings,
                            QVector<float> &samples, int &sampleRate, QString &error);
    QString textWithVoiceInstruction(const QString &text, const QVariantMap &settings) const;
    bool applySeed(const QVariantMap &settings, QString &error);

    void *m_session = nullptr;
    QString m_modelPath;
    std::atomic<bool> m_cancelRequested {false};
};

} // namespace LAStudio
