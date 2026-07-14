#pragma once

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace LAStudio {

class SttBackend {
public:
    virtual ~SttBackend() = default;

    virtual bool loadModel(const QString &modelPath, bool useGpu, const QString &runtimePath, QString &error) = 0;
    virtual void unloadModel() = 0;
    virtual void cancelProcessing() {}
    virtual bool transcribe(const QVector<float> &samples,
                            const QString &language,
                            int threads,
                            bool translate,
                            const QVariantMap &settings,
                            QString &fullText,
                            QVariantList &segments,
                            QString &error) = 0;
};

} // namespace LAStudio
