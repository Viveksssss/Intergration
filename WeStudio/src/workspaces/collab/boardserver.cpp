#include "boardserver.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QIODevice>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

#include <utility>

namespace {
constexpr qsizetype kHistoryLimit = 2048;
constexpr qint64 kSocketReadHighWater = 512 * 1024;

QString safeFileStem(const QString &fileName)
{
    QString stem = QFileInfo(fileName).fileName();
    for (QChar &character : stem) {
        if (QStringLiteral("\\/:*?\"<>|").contains(character)) {
            character = QLatin1Char('_');
        }
    }
    return stem.isEmpty() ? QStringLiteral("shared-file") : stem;
}
}

struct BoardServer::UploadState
{
    QString transferId;
    QString fileName;
    QString path;
    quint64 expectedOffset = 0;
    quint64 size = 0;
    QSaveFile *file = nullptr;

    ~UploadState()
    {
        delete file;
    }
};

BoardServer::BoardServer(const QString &storageDir, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_storageDir(storageDir)
{
    if (m_storageDir.isEmpty()) {
        m_storageDir = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation) + QStringLiteral("/received-files");
    }
    connect(m_server, &QTcpServer::newConnection, this, &BoardServer::acceptPending);
}

BoardServer::~BoardServer()
{
    qDeleteAll(m_uploads);
    qDeleteAll(m_parsers);
}

bool BoardServer::start(quint16 port)
{
    if (m_server->isListening()) {
        emit started(m_server->serverPort());
        return true;
    }
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit errorOccurred(QStringLiteral("监听 %1 端口失败：%2")
                               .arg(port)
                               .arg(m_server->errorString()));
        return false;
    }
    emit started(m_server->serverPort());
    return true;
}

void BoardServer::stop()
{
    m_server->close();
    qDeleteAll(m_uploads);
    m_uploads.clear();
    const auto sockets = m_parsers.keys();
    for (QTcpSocket *socket : sockets) {
        socket->disconnectFromHost();
        socket->deleteLater();
        delete m_parsers.take(socket);
    }
    m_readyClients.clear();
    m_clientNames.clear();
    m_history.clear();
    m_state.clear();
    m_nextSeq = 1;
    emit clientCountChanged(0);
    emit stopped();
}

int BoardServer::clientCount() const
{
    return m_readyClients.size();
}

quint16 BoardServer::port() const
{
    return m_server->serverPort();
}

void BoardServer::acceptPending()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        socket->setReadBufferSize(kSocketReadHighWater);
        auto *parser = new wire::FrameParser;
        m_parsers.insert(socket, parser);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            readClient(socket);
        });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            cancelUpload(socket, QString());
            m_readyClients.remove(socket);
            m_clientNames.remove(socket);
            delete m_parsers.take(socket);
            socket->deleteLater();
            emit clientCountChanged(clientCount());
        });
    }
}

void BoardServer::readClient(QTcpSocket *socket)
{
    auto *parser = m_parsers.value(socket);
    if (!parser) {
        return;
    }
    QVector<wire::Message> messages;
    QString error;
    const bool ok = parser->append(socket->readAll(), &messages, &error);
    if (!ok) {
        reject(socket, error);
        return;
    }
    for (const wire::Message &message : messages) {
        handleMessage(socket, message);
    }
}

void BoardServer::handleMessage(QTcpSocket *socket, const wire::Message &message)
{
    if (message.type == wire::MessageType::Hello) {
        QDataStream stream(message.payload);
        stream.setVersion(QDataStream::Qt_6_2);
        QString clientName;
        stream >> clientName;
        if (stream.status() != QDataStream::Ok || clientName.trimmed().isEmpty()) {
            reject(socket, QStringLiteral("握手负载无效"));
            return;
        }
        m_readyClients.insert(socket);
        m_clientNames.insert(socket, clientName);
        QByteArray payload;
        QDataStream response(&payload, QIODevice::WriteOnly);
        response.setVersion(QDataStream::Qt_6_2);
        response << quint16(wire::kVersion) << clientName << m_nextSeq - 1;
        send(socket, wire::MessageType::Welcome, payload);
        emit clientCountChanged(clientCount());
        return;
    }

    if (!m_readyClients.contains(socket)) {
        reject(socket, QStringLiteral("必须先完成握手"));
        return;
    }

    if (message.type == wire::MessageType::Event) {
        BoardEvent event;
        if (!BoardEvent::fromPayload(message.payload, &event)) {
            reject(socket, QStringLiteral("白板事件负载无效"));
            return;
        }
        event.seq = m_nextSeq++;
        m_history.append(event);
        m_state.apply(event);
        if (m_history.size() > kHistoryLimit) {
            m_history.remove(0, m_history.size() - kHistoryLimit);
        }
        broadcastEvent(event);
        return;
    }

    if (message.type == wire::MessageType::ResumeRequest) {
        QDataStream stream(message.payload);
        stream.setVersion(QDataStream::Qt_6_2);
        quint64 lastSeq = 0;
        stream >> lastSeq;
        if (stream.status() != QDataStream::Ok) {
            reject(socket, QStringLiteral("续传请求无效"));
            return;
        }
        replayFrom(socket, lastSeq);
        return;
    }

    if (message.type == wire::MessageType::FileOffer) {
        handleFileOffer(socket, message.payload);
        return;
    }
    if (message.type == wire::MessageType::FileChunk) {
        handleFileChunk(socket, message.payload);
    }
}

void BoardServer::replayFrom(QTcpSocket *socket, quint64 lastSeq)
{
    const quint64 highestSeq = m_nextSeq - 1;
    if (lastSeq < highestSeq) {
        const bool historyCovers =
            !m_history.isEmpty() && m_history.constFirst().seq <= lastSeq + 1;
        if (historyCovers) {
            for (const BoardEvent &event : std::as_const(m_history)) {
                if (event.seq > lastSeq) {
                    sendEventTo(socket, event);
                }
            }
        } else {
            const QVector<BoardEvent> snapshot = m_state.snapshot();
            for (const BoardEvent &event : snapshot) {
                if (event.seq > lastSeq) {
                    sendEventTo(socket, event);
                }
            }
        }
    }
    sendResumeComplete(socket);
}

void BoardServer::broadcastEvent(const BoardEvent &event)
{
    const QByteArray payload = event.toPayload();
    for (QTcpSocket *client : std::as_const(m_readyClients)) {
        send(client, wire::MessageType::Event, payload);
    }
}

void BoardServer::sendEventTo(QTcpSocket *socket, const BoardEvent &event)
{
    send(socket, wire::MessageType::Event, event.toPayload());
}

void BoardServer::sendResumeComplete(QTcpSocket *socket)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream << quint64(m_nextSeq - 1);
    send(socket, wire::MessageType::ResumeComplete, payload);
}

void BoardServer::handleFileOffer(QTcpSocket *socket, const QByteArray &payload)
{
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_2);
    QString transferId;
    QString fileName;
    quint64 size = 0;
    stream >> transferId >> fileName >> size;
    if (stream.status() != QDataStream::Ok
        || QUuid(transferId).isNull()
        || size == 0) {
        sendFileCancel(socket, transferId, QStringLiteral("文件报头无效"));
        return;
    }
    if (m_uploads.contains(socket)) {
        sendFileCancel(socket, transferId, QStringLiteral("已有文件正在上传"));
        return;
    }
    if (!QDir().mkpath(m_storageDir)) {
        sendFileCancel(socket, transferId, QStringLiteral("无法创建接收目录"));
        return;
    }

    fileName = safeFileStem(fileName);
    const QString path = QDir(m_storageDir).filePath(transferId + QLatin1Char('-') + fileName);
    auto *upload = new UploadState;
    upload->transferId = transferId;
    upload->fileName = fileName;
    upload->path = path;
    upload->size = size;
    upload->file = new QSaveFile(path);
    if (!upload->file->open(QIODevice::WriteOnly)) {
        const QString error = upload->file->errorString();
        delete upload;
        sendFileCancel(socket, transferId, QStringLiteral("无法写入文件：%1").arg(error));
        return;
    }
    m_uploads.insert(socket, upload);

    QByteArray response;
    QDataStream out(&response, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_2);
    out << transferId;
    send(socket, wire::MessageType::FileAccept, response);
}

void BoardServer::handleFileChunk(QTcpSocket *socket, const QByteArray &payload)
{
    UploadState *upload = m_uploads.value(socket);
    QDataStream stream(payload);
    stream.setVersion(QDataStream::Qt_6_2);
    QString transferId;
    quint64 offset = 0;
    QByteArray data;
    stream >> transferId >> offset >> data;
    if (!upload || stream.status() != QDataStream::Ok
        || transferId != upload->transferId
        || offset != upload->expectedOffset
        || data.isEmpty()
        || quint64(data.size()) > upload->size - upload->expectedOffset) {
        cancelUpload(socket, QStringLiteral("文件分块序号或长度无效"));
        return;
    }

    const qint64 written = upload->file->write(data);
    if (written != data.size()) {
        cancelUpload(socket, QStringLiteral("写入文件失败：%1")
                                   .arg(upload->file->errorString()));
        return;
    }
    upload->expectedOffset += quint64(data.size());
    const bool complete = upload->expectedOffset == upload->size;

    if (!complete) {
        QByteArray ackPayload;
        QDataStream ack(&ackPayload, QIODevice::WriteOnly);
        ack.setVersion(QDataStream::Qt_6_2);
        ack << transferId << upload->expectedOffset << false;
        send(socket, wire::MessageType::FileAck, ackPayload);
        return;
    }

    const QString fileName = upload->fileName;
    const QString path = upload->path;
    const quint64 size = upload->size;
    const QString uploader = m_clientNames.value(socket, QStringLiteral("访客"));
    if (!upload->file->commit()) {
        const QString error = upload->file->errorString();
        cancelUpload(socket, QStringLiteral("提交文件失败：%1").arg(error));
        return;
    }
    delete upload;
    m_uploads.remove(socket);

    QByteArray ackPayload;
    QDataStream ack(&ackPayload, QIODevice::WriteOnly);
    ack.setVersion(QDataStream::Qt_6_2);
    ack << transferId << size << true;
    send(socket, wire::MessageType::FileAck, ackPayload);
    emit fileStored(fileName, path, qint64(size));

    BoardEvent notice;
    notice.kind = EventKind::Chat;
    notice.seq = m_nextSeq++;
    notice.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    notice.author = uploader;
    notice.text = QStringLiteral("已上传文件 %1（%2 KiB）")
                      .arg(fileName)
                      .arg((size + 1023) / 1024);
    m_history.append(notice);
    m_state.apply(notice);
    if (m_history.size() > kHistoryLimit) {
        m_history.remove(0, m_history.size() - kHistoryLimit);
    }
    broadcastEvent(notice);
}

void BoardServer::cancelUpload(QTcpSocket *socket, const QString &reason)
{
    UploadState *upload = m_uploads.take(socket);
    if (!upload) {
        return;
    }
    const QString transferId = upload->transferId;
    delete upload;
    if (!reason.isEmpty()) {
        sendFileCancel(socket, transferId, reason);
        emit errorOccurred(reason);
    }
}

void BoardServer::sendFileCancel(QTcpSocket *socket, const QString &transferId,
                                 const QString &reason)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream << transferId << reason;
    send(socket, wire::MessageType::FileCancel, payload);
}

void BoardServer::send(QTcpSocket *socket, wire::MessageType type,
                       const QByteArray &payload)
{
    const QByteArray frame = wire::encode(type, payload);
    if (!frame.isEmpty()) {
        socket->write(frame);
    }
}

void BoardServer::reject(QTcpSocket *socket, const QString &message)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    stream << message;
    send(socket, wire::MessageType::Error, payload);
    socket->disconnectFromHost();
    emit errorOccurred(message);
}
