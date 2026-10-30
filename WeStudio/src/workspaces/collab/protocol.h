#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QVector>

namespace wire {

constexpr quint16 kMagic = 0x4c42;
constexpr quint16 kVersion = 3;
constexpr quint32 kMaxPayload = 8U * 1024U * 1024U;

enum class MessageType : quint8
{
    Hello = 1,
    Welcome = 2,
    Event = 3,
    Error = 4,
    ResumeRequest = 5,
    ResumeComplete = 6,
    FileOffer = 7,
    FileAccept = 8,
    FileChunk = 9,
    FileAck = 10,
    FileCancel = 11
};

struct Message
{
    MessageType type = MessageType::Error;
    QByteArray payload;
};

QByteArray encode(MessageType type, const QByteArray &payload);

class FrameParser
{
public:
    bool append(const QByteArray &bytes, QVector<Message> *messages, QString *error);
    void clear();

private:
    QByteArray m_buffer;
};

} // namespace wire

Q_DECLARE_METATYPE(wire::Message)
