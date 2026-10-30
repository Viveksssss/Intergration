#include "boardclient.h"

#include "filechunkreader.h"

#include <QDataStream>
#include <QFileInfo>
#include <QIODevice>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

namespace {
constexpr qint64 kSocketReadHighWater = 512 * 1024;
constexpr qint64 kSocketWriteHighWater = 512 * 1024;
}

struct BoardClient::ActiveUpload
{
    QString transferId;
    QString fileName;
    QString path;
    quint64 size = 0;
    quint64 nextOffset = 0;
    FileChunkReader *reader = nullptr;
};

BoardClient::BoardClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_reconnectTimer(new QTimer(this))
{
    m_socket->setReadBufferSize(kSocketReadHighWater);
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(1200);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (!m_reconnectEnabled
            || m_socket->state() != QAbstractSocket::UnconnectedState) {
            return;
        }
        m_parser.clear();
        m_socket->connectToHost(m_host, m_port);
    });

    connect(m_socket, &QTcpSocket::connected, this, &BoardClient::sendHello);
    connect(m_socket, &QTcpSocket::readyRead, this, &BoardClient::readServerData);
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        m_welcomeReceived = false;
        m_resumeCompleted = false;
        m_replaying = false;
        cleanupUpload();
        emit connectedChanged(false);
        scheduleReconnect();
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
                emit errorOccurred(m_socket->errorString());
                if (m_socket->state() == QAbstractSocket::UnconnectedState) {
                    scheduleReconnect();
                }
            });
}

BoardClient::~BoardClient()
{
    m_reconnectEnabled = false;
    cleanupUpload();
}

bool BoardClient::connectToServer(const QString &host, quint16 port,
                                  const QString &name)
{
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("昵称不能为空"));
        return false;
    }
    m_reconnectEnabled = false;
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    cleanupUpload();
    m_name = name.trimmed();
    m_host = host;
    m_port = port;
    m_reconnectEnabled = true;
    m_welcomeReceived = false;
    m_resumeCompleted = false;
    m_replaying = false;
    m_parser.clear();
    m_socket->connectToHost(m_host, m_port);
    return true;
}

void BoardClient::disconnectFromServer()
{
    m_reconnectEnabled = false;
    m_reconnectTimer->stop();
    m_welcomeReceived = false;
    m_resumeCompleted = false;
    m_replaying = false;
    cleanupUpload();
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        emit connectedChanged(false);
    } else {
        m_socket->disconnectFromHost();
    }
}

bool BoardClient::isConnected() const
{
    return m_resumeCompleted
           && m_socket->state() == QAbstractSocket::ConnectedState;
}

quint64 BoardClient::lastAppliedSeq() const
{
    return m_lastSeq;
}

void BoardClient::sendEvent(const BoardEvent &event)
{
    if (!isConnected() || !event.isValid()) {
        emit errorOccurred(QStringLiteral("尚未完成握手或续传，事件未发送"));
        return;
    }
    BoardEvent outgoing = event;
    outgoing.seq = 0;
    m_socket->write(wire::encode(wire::MessageType::Event,
                                outgoing.toPayload()));
}

void BoardClient::sendFile(const QString &path)
{
    if (!isConnected()) {
        emit errorOccurred(QStringLiteral("连接未就绪，无法发送文件"));
        return;
    }
    if (m_upload) {
        emit errorOccurred(QStringLiteral("已有文件正在上传"));
        return;
    }
    const QFileInfo info(path);
    if (!info.isFile() || info.size() <= 0) {
        emit errorOccurred(QStringLiteral("请选择非空普通文件"));
        return;
    }

    auto *upload = new ActiveUpload;
    upload->transferId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    upload->fileName = info.fileName();
    upload->path = info.absoluteFilePath();
    upload->size = quint64(info.size());
    upload->reader = new FileChunkReader(upload->path);
    connect(upload->reader, &FileChunkReader::chunkReady,
            this, &BoardClient::handleFileChunk, Qt::QueuedConnection);
    connect(upload->reader, &FileChunkReader::readFailed, this,
            [this](const QString &message) {
                emit errorOccurred(message);
                cleanupUpload();
            }, Qt::QueuedConnection);
    upload->reader->start();
    m_upload = upload;

    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream << m_upload->transferId << m_upload->fileName << m_upload->size;
    m_socket->write(wire::encode(wire::MessageType::FileOffer, payload));
    emit fileProgress(m_upload->fileName, 0, qint64(m_upload->size));
}

void BoardClient::sendHello()
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream << m_name;
    m_socket->write(wire::encode(wire::MessageType::Hello, payload));
}

void BoardClient::sendResumeRequest()
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream << m_lastSeq;
    m_socket->write(wire::encode(wire::MessageType::ResumeRequest, payload));
}

void BoardClient::scheduleReconnect()
{
    if (m_reconnectEnabled && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void BoardClient::readServerData()
{
    QVector<wire::Message> messages;
    QString error;
    if (!m_parser.append(m_socket->readAll(), &messages, &error)) {
        emit errorOccurred(error);
        m_socket->disconnectFromHost();
        return;
    }
    for (const wire::Message &message : messages) {
        handleMessage(message);
    }
}

void BoardClient::handleMessage(const wire::Message &message)
{
    if (message.type == wire::MessageType::Welcome) {
        m_welcomeReceived = true;
        m_replaying = true;
        sendResumeRequest();
        return;
    }
    if (message.type == wire::MessageType::Event) {
        BoardEvent event;
        if (!BoardEvent::fromPayload(message.payload, &event)) {
            emit errorOccurred(QStringLiteral("收到无效白板事件"));
            return;
        }
        if (event.seq != 0 && event.seq <= m_lastSeq) {
            return;
        }
        if (!m_replaying) {
            m_lastSeq = event.seq;
        }
        emit eventReceived(event);
        return;
    }
    if (message.type == wire::MessageType::ResumeComplete) {
        QDataStream stream(message.payload);
        stream.setVersion(QDataStream::Qt_6_2);
        quint64 highestSeq = 0;
        stream >> highestSeq;
        if (stream.status() != QDataStream::Ok) {
            emit errorOccurred(QStringLiteral("续传完成帧无效"));
            return;
        }
        m_lastSeq = highestSeq;
        m_replaying = false;
        m_resumeCompleted = true;
        emit connectedChanged(true);
        return;
    }
    if (message.type == wire::MessageType::FileAccept) {
        QDataStream stream(message.payload);
        stream.setVersion(QDataStream::Qt_6_2);
        QString transferId;
        stream >> transferId;
        if (m_upload && transferId == m_upload->transferId) {
            m_upload->reader->requestNextChunk();
        }
        return;
    }
    if (message.type == wire::MessageType::FileAck) {
        QDataStream stream(message.payload);
        stream.setVersion(QDataStream::Qt_6_2);
        QString transferId;
        quint64 nextOffset = 0;
        bool complete = false;
        stream >> transferId >> nextOffset >> complete;
        if (!m_upload || transferId != m_upload->transferId
            || stream.status() != QDataStream::Ok
            || nextOffset > m_upload->size) {
            emit errorOccurred(QStringLiteral("文件确认帧无效"));
            return;
        }
        m_upload->nextOffset = nextOffset;
        emit fileProgress(m_upload->fileName, qint64(nextOffset),
                          qint64(m_upload->size));
        if (complete && nextOffset == m_upload->size) {
            const QString fileName = m_upload->fileName;
            cleanupUpload();
            emit fileCompleted(fileName);
        } else {
            m_upload->reader->requestNextChunk();
        }
        return;
    }
    if (message.type == wire::MessageType::FileCancel) {
        QDataStream stream(message.payload);
        stream.setVersion(QDataStream::Qt_6_2);
        QString transferId;
        QString reason;
        stream >> transferId >> reason;
        if (m_upload && transferId == m_upload->transferId) {
            cleanupUpload();
        }
        emit errorOccurred(reason);
        return;
    }
    if (message.type == wire::MessageType::Error) {
        QDataStream stream(message.payload);
        stream.setVersion(QDataStream::Qt_6_2);
        QString error;
        stream >> error;
        emit errorOccurred(error);
    }
}

void BoardClient::handleFileChunk(quint64 offset, const QByteArray &data, bool)
{
    if (!m_upload || !isConnected()
        || offset != m_upload->nextOffset
        || m_socket->bytesToWrite() > kSocketWriteHighWater) {
        emit errorOccurred(QStringLiteral("文件分块状态或发送缓冲异常"));
        cleanupUpload();
        return;
    }
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream << m_upload->transferId << offset << data;
    m_socket->write(wire::encode(wire::MessageType::FileChunk, payload));
}

void BoardClient::cleanupUpload()
{
    if (!m_upload) {
        return;
    }
    if (m_upload->reader) {
        disconnect(m_upload->reader, nullptr, this, nullptr);
        m_upload->reader->cancel();
        m_upload->reader->wait();
        delete m_upload->reader;
    }
    delete m_upload;
    m_upload = nullptr;
}
