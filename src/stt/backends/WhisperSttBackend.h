#pragma once

#include "SttBackend.h"
#include <atomic>

struct whisper_context;

namespace LAStudio {

class WhisperSttBackend : public SttBackend {
public:
    ~WhisperSttBackend() override;

    bool loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath, QString &error) override;
    void unloadModel() override;
    void cancelProcessing() override;
    bool transcribe(const QVector<float> &samples,
                    const QString &language,
                    int threads,
                    bool translate,
                    const QVariantMap &settings,
                    QString &fullText,
                    QVariantList &segments,
                    QString &error) override;

private:
    whisper_context *m_ctx = nullptr;
    std::atomic<bool> m_abort {false};
    bool m_useGpu = false;
};

} // namespace LAStudio
