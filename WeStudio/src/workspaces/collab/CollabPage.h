#pragma once

#include "model.h"

#include <QMainWindow>
#include <QVector>

class BoardService;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QProgressBar;
class QThread;
class QTimer;
class WhiteboardWidget;

class CollabPage : public QMainWindow
{
    Q_OBJECT

public:
    explicit CollabPage(QWidget *parent = nullptr);
    ~CollabPage() override;

signals:
    void startServerRequested(quint16 port);
    void stopServerRequested();
    void connectRequested(const QString &host, quint16 port, const QString &name);
    void disconnectRequested();
    void eventSendRequested(const BoardEvent &event);
    void fileSendRequested(const QString &path);

private:
    void buildUi();
    void hostRoom();
    void joinRoom();
    void sendChat();
    bool sendBoardEvent(const BoardEvent &event);
    void chooseFile();
    void receiveBoardEvent(const BoardEvent &event);
    void flushPendingEvents();
    void setConnectionState(bool connected);
    QString currentName() const;

    BoardService *m_service = nullptr;
    QThread *m_networkThread = nullptr;
    QTimer *m_uiFlushTimer = nullptr;
    QVector<BoardEvent> m_pendingEvents;
    WhiteboardWidget *m_board = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QLineEdit *m_chatEdit = nullptr;
    QPlainTextEdit *m_chatView = nullptr;
    QPushButton *m_hostButton = nullptr;
    QPushButton *m_joinButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QPushButton *m_fileButton = nullptr;
    QProgressBar *m_fileProgress = nullptr;
    bool m_hosting = false;
};
