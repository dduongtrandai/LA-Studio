#pragma once

#include <QByteArray>
#include <QCborMap>
#include <QIODevice>
#include <QString>

#include <optional>

namespace LAStudio {

// Versioned, local-only protocol shared by LA Studio and RuntimeHost. The
// framing is deliberately independent of JSON so audio/control messages do
// not require text encoding or base64 copies.
constexpr quint32 kRuntimeHostMagic = 0x4C415248; // "LARH"
constexpr quint16 kRuntimeHostProtocolMajor = 1;
constexpr quint16 kRuntimeHostProtocolMinor = 0;
constexpr qsizetype kRuntimeHostMaxPayloadBytes = 64 * 1024 * 1024;

enum class RuntimeHostMessage : quint16 {
    Hello = 1,
    HelloAck = 2,
    Load = 3,
    LoadResult = 4,
    Infer = 5,
    Progress = 6,
    Completed = 7,
    Error = 8,
    Cancel = 9,
    Cancelled = 10,
    Unload = 11,
    UnloadResult = 12,
    Ping = 13,
    Pong = 14,
    Shutdown = 15,
    Log = 16,
};

struct RuntimeHostFrame {
    RuntimeHostMessage message = RuntimeHostMessage::Error;
    quint32 flags = 0;
    quint64 requestId = 0;
    QByteArray payload;
};

QByteArray encodeRuntimeHostFrame(RuntimeHostMessage message,
                                  quint64 requestId,
                                  const QByteArray &payload = {},
                                  quint32 flags = 0);

class RuntimeHostFrameParser final {
public:
    void append(const QByteArray &bytes);
    std::optional<RuntimeHostFrame> takeNext(QString *error = nullptr);
    void clear() { m_buffer.clear(); }

private:
    QByteArray m_buffer;
};

QByteArray encodeRuntimeHostCbor(const QCborMap &map);
bool decodeRuntimeHostCbor(const QByteArray &payload, QCborMap *map, QString *error = nullptr);

} // namespace LAStudio
