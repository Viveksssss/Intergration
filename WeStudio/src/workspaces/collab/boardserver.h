#pragma once

#include "protocol.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#include "model.h"

class QTcpServer;
class QTcpSocket;

class BoardServer : public QObject
{
    Q_OBJECT

public:
    explicit BoardServer(const QString &storageDir = QString(),
                         QObject *parent = nullptr);
    ~BoardServer() override;

    bool start(quint16 port);
    void stop();
    int clientCount() const;
    quint16 port() const;

signals:
    void started(quint16 port);
    void stopped();
    void clientCountChanged(int count);
    void errorOccurred(const QString &message);
    void fileStored(const QString &fileName, const QString &path, qint64 bytes);

private:
    struct UploadState;

    void acceptPending();
    void readClient(QTcpSocket *socket);
    void handleMessage(QTcpSocket *socket, const wire::Message &message);
    void send(QTcpSocket *socket, wire::MessageType type, const QByteArray &payload);
    void reject(QTcpSocket *socket, const QString &message);
    void replayFrom(QTcpSocket *socket, quint64 lastSeq);
    void broadcastEvent(const BoardEvent &event);
    void sendEventTo(QTcpSocket *socket, const BoardEvent &event);
    void sendResumeComplete(QTcpSocket *socket);
    void handleFileOffer(QTcpSocket *socket, const QByteArray &payload);
    void handleFileChunk(QTcpSocket *socket, const QByteArray &payload);
    void cancelUpload(QTcpSocket *socket, const QString &reason);
    void sendFileCancel(QTcpSocket *socket, const QString &transferId,
                        const QString &reason);

    QTcpServer *m_server = nullptr;
    QString m_storageDir;
    QHash<QTcpSocket *, wire::FrameParser *> m_parsers;
    QHash<QTcpSocket *, QString> m_clientNames;
    QHash<QTcpSocket *, UploadState *> m_uploads;
    QSet<QTcpSocket *> m_readyClients;
    QVector<BoardEvent> m_history;
    BoardModelState m_state;
    quint64 m_nextSeq = 1;
};
