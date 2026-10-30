#include "boardservice.h"

#include "boardclient.h"
#include "boardserver.h"

BoardService::BoardService(QObject *parent)
    : QObject(parent)
    , m_server(new BoardServer(QString(), this))
    , m_client(new BoardClient(this))
{
    connect(m_server, &BoardServer::started, this, &BoardService::serverStarted);
    connect(m_server, &BoardServer::stopped, this, &BoardService::serverStopped);
    connect(m_server, &BoardServer::clientCountChanged,
            this, &BoardService::clientCountChanged);
    connect(m_server, &BoardServer::errorOccurred,
            this, &BoardService::errorOccurred);
    connect(m_client, &BoardClient::connectedChanged,
            this, &BoardService::connectedChanged);
    connect(m_client, &BoardClient::eventReceived,
            this, &BoardService::eventReceived);
    connect(m_client, &BoardClient::errorOccurred,
            this, &BoardService::errorOccurred);
    connect(m_server, &BoardServer::fileStored,
            this, &BoardService::fileStored);
    connect(m_client, &BoardClient::fileProgress,
            this, &BoardService::fileProgress);
    connect(m_client, &BoardClient::fileCompleted,
            this, &BoardService::fileCompleted);
}

void BoardService::startServer(quint16 port)
{
    m_server->start(port);
}

void BoardService::stopServer()
{
    m_server->stop();
}

void BoardService::connectToServer(const QString &host, quint16 port,
                                   const QString &name)
{
    m_client->connectToServer(host, port, name);
}

void BoardService::disconnectFromServer()
{
    m_client->disconnectFromServer();
}

void BoardService::sendEvent(const BoardEvent &event)
{
    m_client->sendEvent(event);
}

void BoardService::sendFile(const QString &path)
{
    m_client->sendFile(path);
}
