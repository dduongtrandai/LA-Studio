#pragma once

#include <QCborMap>
#include <QCborValue>
#include <QVector>
#include <QMetaType>

#include <memory>

#include <functional>

namespace LAStudio {

class RuntimeHostAdapter {
public:
    struct Result {
        QCborMap payload;
        QVector<float> samples;
        int sampleRate = 0;
    };
    using ProgressCallback = std::function<bool(int current,
                                                int total,
                                                const QString &stage,
                                                int chunkIndex,
                                                int chunkCount)>;
    virtual ~RuntimeHostAdapter() = default;

    virtual bool load(const QCborMap &configuration, QCborValue *schema, QString *error) = 0;
    virtual void unload() = 0;
    virtual bool infer(const QCborMap &request,
                       const QVector<float> &referenceSamples,
                       QVector<float> *samples,
                       int *sampleRate,
                       QString *error) = 0;
    // Generic result channel. Legacy audio adapters only need infer(); text
    // and structured adapters can override this and fill payload.
    virtual bool execute(const QCborMap &request,
                         const QVector<float> &referenceSamples,
                         Result *result,
                         QString *error);
    virtual void cancel() = 0;
    virtual void setProgressCallback(ProgressCallback callback) = 0;
};

std::unique_ptr<RuntimeHostAdapter> createRuntimeHostAdapter(const QString &adapterId);

} // namespace LAStudio

Q_DECLARE_METATYPE(LAStudio::RuntimeHostAdapter::Result)
