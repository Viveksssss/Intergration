#pragma once

#include "model.h"
#include "protocol.h"

#include <QObject>

class QTcpSocket;
class QTimer;
class FileChunkReader;

class BoardClient : public QObject
{
    Q_OBJECT

public:
    explicit BoardClient(QObject *parent = nullptr);
    ~BoardClient() override;

    bool connectToServer(const QString &host, quint16 port, const QString &name);
    void disconnectFromServer();
    bool isConnected() const;
    quint64 lastAppliedSeq() const;
    void sendEvent(const BoardEvent &event);
    void sendFile(const QString &path);

signals:
    void connectedChanged(bool connected);
    void eventReceived(const BoardEvent &event);
    void errorOccurred(const QString &message);
    void fileProgress(const QString &fileName, qint64 sent, qint64 total);
    void fileCompleted(const QString &fileName);

private:
    struct ActiveUpload;

    void sendHello();
    void sendResumeRequest();
    void scheduleReconnect();
    void readServerData();
    void handleMessage(const wire::Message &message);
    void beginFileTransfer();
    void handleFileChunk(quint64 offset, const QByteArray &data, bool last);
    void cleanupUpload();

    QTcpSocket *m_socket = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    wire::FrameParser m_parser;
    QString m_name;
    QString m_host;
    quint16 m_port = 0;
    quint64 m_lastSeq = 0;
    bool m_welcomeReceived = false;
    bool m_resumeCompleted = false;
    bool m_replaying = false;
    bool m_reconnectEnabled = false;
    ActiveUpload *m_upload = nullptr;
};
