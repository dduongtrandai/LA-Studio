#pragma once
#include "TtsBackend.h"
#include <atomic>

namespace LAStudio {

struct kokoro_vi_context;

class KokoroVietnameseBackend : public TtsBackend {
public:
    KokoroVietnameseBackend() = default;
    ~KokoroVietnameseBackend() override;

    bool load(const QVariantMap &config, QString &error, QVariantList &schema) override;
    void unload() override;
    void cancelProcessing() override;
    bool synthesize(const QString &text, float speed, const QVariantMap &settings,
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings,
                    QVector<float> &samples, int &sampleRate, QString &error) override;

private:
    bool m_loaded = false;
    QString m_runtimePath;
    QString m_modelDir;
    QString m_currentVoiceId;
    kokoro_vi_context *m_context = nullptr;
    std::atomic<bool> m_cancelRequested {false};
};

} // namespace LAStudio
