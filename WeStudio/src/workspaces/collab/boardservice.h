#pragma once

#include "model.h"

#include <QObject>

class BoardClient;
class BoardServer;

class BoardService : public QObject
{
    Q_OBJECT

public:
    explicit BoardService(QObject *parent = nullptr);

public slots:
    void startServer(quint16 port);
    void stopServer();
    void connectToServer(const QString &host, quint16 port, const QString &name);
    void disconnectFromServer();
    void sendEvent(const BoardEvent &event);
    void sendFile(const QString &path);

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void clientCountChanged(int count);
    void connectedChanged(bool connected);
    void eventReceived(const BoardEvent &event);
    void statusMessage(const QString &message);
    void errorOccurred(const QString &message);
    void fileProgress(const QString &fileName, qint64 sent, qint64 total);
    void fileCompleted(const QString &fileName);
    void fileStored(const QString &fileName, const QString &path, qint64 bytes);

private:
    BoardServer *m_server = nullptr;
    BoardClient *m_client = nullptr;
};
