#pragma once

#include "TtsBackend.h"
#include <atomic>
#include <functional>
#include <QHash>

namespace LAStudio {

struct vieneu_progress;

class VieneuTtsBackend : public TtsBackend {
public:
    VieneuTtsBackend() = default;
    ~VieneuTtsBackend() override;

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
    static void handleProgress(const vieneu_progress *progress, void *userData);
    QString resolveVieneuV3Style(const QVariantMap &settings) const;

    void *m_context = nullptr;
    bool m_useAbiV2 = false;
    QString m_pipelineProfile;
    QString m_vieneuV3DefaultVoiceId;
    QHash<QString, QString> m_vieneuV3VoiceStyles;
    std::atomic<bool> m_cancelRequested {false};
    std::function<bool(int current, int total, const QString &stage, int chunkIndex, int chunkCount)> m_progressCallback;
};

} // namespace LAStudio
