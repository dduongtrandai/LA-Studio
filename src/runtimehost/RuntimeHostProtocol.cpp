#include "RuntimeHostProtocol.h"

#include <QCborValue>
#include <QDataStream>

namespace LAStudio {
namespace {

constexpr qsizetype kHeaderSize = sizeof(quint32) + sizeof(quint16) + sizeof(quint16)
    + sizeof(quint16) + sizeof(quint32) + sizeof(quint64) + sizeof(quint64);

QByteArray readBytes(const QByteArray &buffer, qsizetype offset, qsizetype size)
{
    return buffer.sliced(offset, size);
}

} // namespace

QByteArray encodeRuntimeHostFrame(RuntimeHostMessage message,
                                  quint64 requestId,
                                  const QByteArray &payload,
                                  quint32 flags)
{
    QByteArray frame;
    frame.reserve(kHeaderSize + payload.size());
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << kRuntimeHostMagic
           << kRuntimeHostProtocolMajor
           << kRuntimeHostProtocolMinor
           << static_cast<quint16>(message)
           << flags
           << requestId
           << static_cast<quint64>(payload.size());
    frame.append(payload);
    return frame;
}

void RuntimeHostFrameParser::append(const QByteArray &bytes)
{
    if (!bytes.isEmpty()) {
        m_buffer.append(bytes);
    }
}

std::optional<RuntimeHostFrame> RuntimeHostFrameParser::takeNext(QString *error)
{
    if (m_buffer.size() < kHeaderSize) {
        return std::nullopt;
    }

    QByteArray header = m_buffer.left(kHeaderSize);
    QDataStream stream(&header, QIODevice::ReadOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0;
    quint16 major = 0;
    quint16 minor = 0;
    quint16 rawMessage = 0;
    quint32 flags = 0;
    quint64 requestId = 0;
    quint64 payloadSize = 0;
    stream >> magic >> major >> minor >> rawMessage >> flags >> requestId >> payloadSize;

    if (magic != kRuntimeHostMagic) {
        if (error) *error = QStringLiteral("RuntimeHost protocol magic mismatch.");
        m_buffer.clear();
        return std::nullopt;
    }
    if (major != kRuntimeHostProtocolMajor) {
        if (error) *error = QStringLiteral("RuntimeHost protocol major version mismatch.");
        m_buffer.clear();
        return std::nullopt;
    }
    Q_UNUSED(minor);
    if (payloadSize > static_cast<quint64>(kRuntimeHostMaxPayloadBytes)) {
        if (error) *error = QStringLiteral("RuntimeHost payload is too large.");
        m_buffer.clear();
        return std::nullopt;
    }
    if (m_buffer.size() < kHeaderSize + static_cast<qsizetype>(payloadSize)) {
        return std::nullopt;
    }

    RuntimeHostFrame result;
    result.message = static_cast<RuntimeHostMessage>(rawMessage);
    result.flags = flags;
    result.requestId = requestId;
    result.payload = readBytes(m_buffer, kHeaderSize, static_cast<qsizetype>(payloadSize));
    m_buffer.remove(0, kHeaderSize + static_cast<qsizetype>(payloadSize));
    return result;
}

QByteArray encodeRuntimeHostCbor(const QCborMap &map)
{
    return QCborValue(map).toCbor();
}

bool decodeRuntimeHostCbor(const QByteArray &payload, QCborMap *map, QString *error)
{
    if (!map) {
        if (error) *error = QStringLiteral("RuntimeHost CBOR destination is null.");
        return false;
    }
    QCborParserError parserError;
    const QCborValue value = QCborValue::fromCbor(payload, &parserError);
    if (parserError.error != QCborError::NoError || !value.isMap()) {
        if (error) *error = QStringLiteral("RuntimeHost payload is not a valid CBOR map.");
        return false;
    }
    *map = value.toMap();
    return true;
}

} // namespace LAStudio
