#include "protocol.h"

#include <QDataStream>
#include <QIODevice>

namespace wire {
namespace {
constexpr int kHeaderBytes = 9;

void setStableVersion(QDataStream &stream)
{
    stream.setVersion(QDataStream::Qt_6_2);
}
}

QByteArray encode(MessageType type, const QByteArray &payload)
{
    if (payload.size() < 0 || quint32(payload.size()) > kMaxPayload) {
        return {};
    }

    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    setStableVersion(stream);
    stream << kMagic
           << kVersion
           << quint8(type)
           << quint32(payload.size());
    frame.append(payload);
    return frame;
}

bool FrameParser::append(const QByteArray &bytes, QVector<Message> *messages,
                         QString *error)
{
    if (!messages) {
        return false;
    }
    m_buffer.append(bytes);

    while (m_buffer.size() >= kHeaderBytes) {
        QDataStream header(m_buffer);
        setStableVersion(header);
        quint16 magic = 0;
        quint16 version = 0;
        quint8 rawType = 0;
        quint32 payloadLength = 0;
        header >> magic >> version >> rawType >> payloadLength;

        if (magic != kMagic) {
            m_buffer.clear();
            if (error) {
                *error = QStringLiteral("无效的协议魔数");
            }
            return false;
        }
        if (version != kVersion) {
            m_buffer.clear();
            if (error) {
                *error = QStringLiteral("协议版本不匹配：服务器 v%1，客户端 v%2")
                             .arg(kVersion)
                             .arg(version);
            }
            return false;
        }
        if (payloadLength > kMaxPayload) {
            m_buffer.clear();
            if (error) {
                *error = QStringLiteral("负载超过 8 MiB 上限");
            }
            return false;
        }
        if (m_buffer.size() < kHeaderBytes + int(payloadLength)) {
            return true;
        }

        Message message;
        message.type = static_cast<MessageType>(rawType);
        message.payload = m_buffer.mid(kHeaderBytes, int(payloadLength));
        m_buffer.remove(0, kHeaderBytes + int(payloadLength));
        messages->append(message);
    }
    return true;
}

void FrameParser::clear()
{
    m_buffer.clear();
}

} // namespace wire
