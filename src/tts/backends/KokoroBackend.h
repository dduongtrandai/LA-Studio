#pragma once
#include "TtsBackend.h"
#include <atomic>

namespace LAStudio {

class KokoroBackend : public TtsBackend {
public:
    KokoroBackend() = default;
    ~KokoroBackend() override;

    bool load(const QVariantMap &config, QString &error, QVariantList &schema) override;
    void unload() override;
    void cancelProcessing() override;
    bool synthesize(const QString &text, float speed, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;

private:
    void *m_session = nullptr;
    QString m_currentVoicePath;
    QString m_modelPath;
    std::atomic<bool> m_cancelRequested {false};
};

} // namespace LAStudio
