#pragma once
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QVector>
#include <functional>

namespace LAStudio {

class TtsBackend {
public:
    virtual ~TtsBackend() = default;

    // Load model and return true if successful, along with the schema for QML UI.
    virtual bool load(const QVariantMap &config, QString &error, QVariantList &schema) = 0;
    
    // Unload resources related to this backend.
    virtual void unload() = 0;

    virtual void setProgressCallback(std::function<bool(int current,
                                                        int total,
                                                        const QString &stage,
                                                        int chunkIndex,
                                                        int chunkCount)> callback)
    {
        Q_UNUSED(callback);
    }

    virtual void cancelProcessing() {}
    
    // Synthesize text to raw float PCM samples.
    virtual bool synthesize(const QString &text, float speed, const QVariantMap &settings, 
                            QVector<float> &samples, int &sampleRate, QString &error) = 0;
                            
    // Clone voice using reference audio and text.
    virtual bool cloneVoice(const QString &text, const QString &referencePath, const QVariantMap &settings, 
                            QVector<float> &samples, int &sampleRate, QString &error) = 0;
};

} // namespace LAStudio
