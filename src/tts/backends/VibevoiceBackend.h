#pragma once
#include "TtsBackend.h"
#include <atomic>

namespace LAStudio {

class VibevoiceBackend : public TtsBackend {
public:
    VibevoiceBackend() = default;
    ~VibevoiceBackend() override;

    bool load(const QVariantMap &config, QString &error, QVariantList &schema) override;
    void unload() override;
    void cancelProcessing() override;
    bool synthesize(const QString &text, float speed, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;

private:
    bool m_loaded = false;
    QString m_modelPath;
    QString m_backendName;
    QString m_currentVoicePath;
    void *m_session = nullptr;
    std::atomic<bool> m_cancelRequested {false};
};

} // namespace LAStudio
