#pragma once
#include "TtsBackend.h"
#include <QByteArray>
#include <atomic>

namespace LAStudio {

class Qwen3Backend : public TtsBackend {
public:
    Qwen3Backend() = default;
    ~Qwen3Backend() override;

    bool load(const QVariantMap &config, QString &error, QVariantList &schema) override;
    void unload() override;
    void cancelProcessing() override;
    bool synthesize(const QString &text, float speed, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;

private:
    void *m_session = nullptr;
    QString m_modelPath;
    bool m_codePredictorBackendEnvOverridden = false;
    bool m_restoreCodePredictorBackendEnv = false;
    QByteArray m_previousCodePredictorBackendEnv;
    std::atomic<bool> m_cancelRequested {false};
};

} // namespace LAStudio
