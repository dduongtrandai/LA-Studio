#pragma once

#include <QSharedMemory>
#include <QVector>
#include <QCborMap>

#include <memory>

namespace LAStudio {

// One-shot float PCM transport. The owner keeps the mapping alive until the
// request response has been consumed; the peer only needs the descriptor.
class RuntimeHostSharedBuffer final {
public:
    RuntimeHostSharedBuffer() = default;
    ~RuntimeHostSharedBuffer();
    RuntimeHostSharedBuffer(const RuntimeHostSharedBuffer &) = delete;
    RuntimeHostSharedBuffer &operator=(const RuntimeHostSharedBuffer &) = delete;

    bool createFromSamples(const QVector<float> &samples,
                           int sampleRate,
                           int channels,
                           QCborMap *descriptor,
                           QString *error = nullptr);
    bool attach(const QCborMap &descriptor, QString *error = nullptr);
    bool copyTo(QVector<float> *samples, QString *error = nullptr) const;

    void detach();
    const QCborMap &descriptor() const { return m_descriptor; }

private:
    std::unique_ptr<QSharedMemory> m_memory;
    QCborMap m_descriptor;
};

} // namespace LAStudio
