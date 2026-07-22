#include "RuntimeHostSharedBuffer.h"

#include <QRandomGenerator>
#include <QCoreApplication>

#include <cstring>
#include <memory>

namespace LAStudio {

RuntimeHostSharedBuffer::~RuntimeHostSharedBuffer()
{
    detach();
}

bool RuntimeHostSharedBuffer::createFromSamples(const QVector<float> &samples,
                                                int sampleRate,
                                                int channels,
                                                QCborMap *descriptor,
                                                QString *error)
{
    if (!descriptor || samples.isEmpty() || sampleRate <= 0 || channels <= 0) {
        if (error) *error = QStringLiteral("Invalid RuntimeHost audio buffer parameters.");
        return false;
    }

    detach();
    const QString key = QStringLiteral("lastudio-audio-%1-%2")
                            .arg(QCoreApplication::applicationPid())
                            .arg(QRandomGenerator::global()->generate64(), 16, 16, QLatin1Char('0'));
    m_memory = std::make_unique<QSharedMemory>(key);
    const qsizetype bytes = samples.size() * static_cast<qsizetype>(sizeof(float));
    if (!m_memory->create(bytes)) {
        if (error) *error = QStringLiteral("Could not create RuntimeHost shared audio buffer: %1")
                                   .arg(m_memory->errorString());
        m_memory.reset();
        return false;
    }
    if (!m_memory->lock()) {
        if (error) *error = m_memory->errorString();
        detach();
        return false;
    }
    std::memcpy(m_memory->data(), samples.constData(), static_cast<size_t>(bytes));
    m_memory->unlock();

    m_descriptor = QCborMap{{QStringLiteral("name"), key},
                            {QStringLiteral("bytes"), static_cast<qint64>(bytes)},
                            {QStringLiteral("samples"), samples.size()},
                            {QStringLiteral("sampleRate"), sampleRate},
                            {QStringLiteral("channels"), channels},
                            {QStringLiteral("format"), QStringLiteral("f32le")}};
    *descriptor = m_descriptor;
    return true;
}

bool RuntimeHostSharedBuffer::attach(const QCborMap &descriptor, QString *error)
{
    detach();
    const QString key = descriptor.value(QStringLiteral("name")).toString();
    const qint64 bytes = descriptor.value(QStringLiteral("bytes")).toInteger();
    if (key.isEmpty() || bytes <= 0 || bytes > 1024LL * 1024LL * 1024LL
        || descriptor.value(QStringLiteral("format")).toString() != QStringLiteral("f32le")) {
        if (error) *error = QStringLiteral("Invalid RuntimeHost shared audio descriptor.");
        return false;
    }
    m_memory = std::make_unique<QSharedMemory>(key);
    if (!m_memory->attach(QSharedMemory::ReadOnly)) {
        if (error) *error = QStringLiteral("Could not attach RuntimeHost shared audio buffer: %1")
                                   .arg(m_memory->errorString());
        m_memory.reset();
        return false;
    }
    m_descriptor = descriptor;
    return true;
}

bool RuntimeHostSharedBuffer::copyTo(QVector<float> *samples, QString *error) const
{
    if (!samples || !m_memory || !m_memory->isAttached()) {
        if (error) *error = QStringLiteral("RuntimeHost shared audio buffer is not attached.");
        return false;
    }
    const qint64 bytes = m_descriptor.value(QStringLiteral("bytes")).toInteger();
    const int count = m_descriptor.value(QStringLiteral("samples")).toInteger();
    if (count <= 0 || bytes != static_cast<qint64>(count) * static_cast<qint64>(sizeof(float))) {
        if (error) *error = QStringLiteral("RuntimeHost shared audio buffer size is invalid.");
        return false;
    }
    if (!m_memory->lock()) {
        if (error) *error = m_memory->errorString();
        return false;
    }
    samples->resize(count);
    std::memcpy(samples->data(), m_memory->constData(), static_cast<size_t>(bytes));
    m_memory->unlock();
    return true;
}

void RuntimeHostSharedBuffer::detach()
{
    if (m_memory && m_memory->isAttached()) m_memory->detach();
    m_memory.reset();
    m_descriptor.clear();
}

} // namespace LAStudio
