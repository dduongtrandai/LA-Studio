#pragma once
#include "TtsBackend.h"
#include <atomic>
#include <functional>

namespace LAStudio {

class OmnivoiceBackend : public TtsBackend {
public:
    OmnivoiceBackend() = default;
    ~OmnivoiceBackend() override;

    bool load(const QVariantMap &config, QString &error, QVariantList &schema) override;
    void unload() override;
    bool synthesize(const QString &text, float speed, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                    QVector<float> &samples, int &sampleRate, QString &error) override;
    void cancelProcessing() override;
    void setProgressCallback(std::function<bool(int current,
                                                int total,
                                                const QString &stage,
                                                int chunkIndex,
                                                int chunkCount)> callback) override;

private:
    static bool handleProgress(int current, int total, const char *stage, int chunkIndex, int chunkCount, void *userData);
    static bool shouldCancel(void *userData);

    void *m_context = nullptr;
    std::atomic<bool> m_cancelRequested {false};
    std::function<bool(int current, int total, const QString &stage, int chunkIndex, int chunkCount)> m_progressCallback;
};

} // namespace LAStudio
