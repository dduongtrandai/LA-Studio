#pragma once

#include "SttBackend.h"

namespace LAStudio {

class QwenSttBackend : public SttBackend {
public:
    QwenSttBackend() = default;
    ~QwenSttBackend() override;

    bool loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath, QString &error) override;
    void unloadModel() override;
    bool transcribe(const QVector<float> &samples,
                    const QString &language,
                    int threads,
                    bool translate,
                    const QVariantMap &settings,
                    QString &fullText,
                    QVariantList &segments,
                    QString &error) override;

private:
    void *m_session = nullptr;
    QString m_modelPath;
};

} // namespace LAStudio
