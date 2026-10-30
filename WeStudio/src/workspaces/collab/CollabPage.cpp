#include "CollabPage.h"

#include "boardservice.h"
#include "whiteboardwidget.h"

#include <QAction>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QUuid>
#include <QVBoxLayout>

#include <utility>

CollabPage::CollabPage(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("局域网协同白板 · 阶段 4"));
    resize(1180, 760);

    m_networkThread = new QThread(this);
    m_service = new BoardService;
    m_service->moveToThread(m_networkThread);

    connect(this, &CollabPage::startServerRequested,
            m_service, &BoardService::startServer, Qt::QueuedConnection);
    connect(this, &CollabPage::stopServerRequested,
            m_service, &BoardService::stopServer, Qt::QueuedConnection);
    connect(this, &CollabPage::connectRequested,
            m_service, &BoardService::connectToServer, Qt::QueuedConnection);
    connect(this, &CollabPage::disconnectRequested,
            m_service, &BoardService::disconnectFromServer, Qt::QueuedConnection);
    connect(this, &CollabPage::eventSendRequested,
            m_service, &BoardService::sendEvent, Qt::QueuedConnection);
    connect(this, &CollabPage::fileSendRequested,
            m_service, &BoardService::sendFile, Qt::QueuedConnection);

    connect(m_service, &BoardService::eventReceived,
            this, &CollabPage::receiveBoardEvent, Qt::QueuedConnection);
    connect(m_service, &BoardService::connectedChanged,
            this, &CollabPage::setConnectionState, Qt::QueuedConnection);
    connect(m_service, &BoardService::errorOccurred, this,
            [this](const QString &message) {
                statusBar()->showMessage(message, 5000);
            }, Qt::QueuedConnection);
    connect(m_service, &BoardService::fileProgress, this,
            [this](const QString &fileName, qint64 sent, qint64 total) {
                m_fileProgress->setVisible(true);
                m_fileProgress->setRange(0, 100);
                m_fileProgress->setValue(
                    total > 0 ? int((sent * 100) / total) : 0);
                statusBar()->showMessage(
                    tr("正在上传 %1：%2 / %3 字节")
                        .arg(fileName).arg(sent).arg(total));
            }, Qt::QueuedConnection);
    connect(m_service, &BoardService::fileCompleted, this,
            [this](const QString &fileName) {
                m_fileProgress->setValue(100);
                statusBar()->showMessage(
                    tr("文件 %1 上传完成").arg(fileName), 5000);
            }, Qt::QueuedConnection);
    connect(m_service, &BoardService::fileStored, this,
            [this](const QString &fileName, const QString &path, qint64 bytes) {
                statusBar()->showMessage(
                    tr("主机已接收 %1（%2 字节）：%3")
                        .arg(fileName).arg(bytes).arg(path), 8000);
            }, Qt::QueuedConnection);
    connect(m_service, &BoardService::serverStarted, this, [this](quint16 port) {
        m_hosting = true;
        m_portEdit->setText(QString::number(port));
        statusBar()->showMessage(tr("房间已监听端口 %1").arg(port));
        emit connectRequested(QStringLiteral("127.0.0.1"), port, currentName());
    }, Qt::QueuedConnection);
    connect(m_service, &BoardService::clientCountChanged, this, [this](int count) {
        if (m_hosting) {
            statusBar()->showMessage(tr("房间在线客户端：%1").arg(count));
        }
    }, Qt::QueuedConnection);
    connect(m_networkThread, &QThread::finished,
            m_service, &QObject::deleteLater);

    m_uiFlushTimer = new QTimer(this);
    m_uiFlushTimer->setSingleShot(true);
    m_uiFlushTimer->setInterval(33);
    connect(m_uiFlushTimer, &QTimer::timeout,
            this, &CollabPage::flushPendingEvents);

    m_networkThread->start();
    buildUi();
}

CollabPage::~CollabPage()
{
    if (!m_networkThread->isRunning()) {
        return;
    }
    QMetaObject::invokeMethod(m_service, &BoardService::disconnectFromServer,
                              Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_service, &BoardService::stopServer,
                              Qt::BlockingQueuedConnection);
    m_networkThread->quit();
    m_networkThread->wait(3000);
}

void CollabPage::buildUi()
{
    auto *toolbar = addToolBar(tr("白板"));
    toolbar->setMovable(false);
    auto *clearAction = toolbar->addAction(tr("清空画布"));
    connect(clearAction, &QAction::triggered, this, [this] {
        m_board->clearCanvas();
        BoardEvent event;
        event.kind = EventKind::Clear;
        event.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        event.author = currentName();
        sendBoardEvent(event);
    });

    m_board = new WhiteboardWidget(this);
    connect(m_board, &WhiteboardWidget::strokeCommitted, this,
            [this](const CanvasStroke &stroke) {
                BoardEvent event;
                event.kind = EventKind::Stroke;
                event.id = stroke.id;
                event.author = stroke.author;
                event.color = stroke.color;
                event.width = stroke.width;
                event.points = stroke.points;
                sendBoardEvent(event);
            });

    auto *side = new QWidget(this);
    auto *sideLayout = new QVBoxLayout(side);

    auto *connectionBox = new QGroupBox(tr("房间"), side);
    auto *form = new QFormLayout(connectionBox);
    m_nameEdit = new QLineEdit(tr("访客"), connectionBox);
    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), connectionBox);
    m_portEdit = new QLineEdit(QStringLiteral("45454"), connectionBox);
    form->addRow(tr("昵称"), m_nameEdit);
    form->addRow(tr("主机"), m_hostEdit);
    form->addRow(tr("端口"), m_portEdit);

    auto *buttons = new QHBoxLayout;
    m_hostButton = new QPushButton(tr("创建房间"), connectionBox);
    m_joinButton = new QPushButton(tr("加入房间"), connectionBox);
    m_disconnectButton = new QPushButton(tr("断开"), connectionBox);
    m_disconnectButton->setEnabled(false);
    m_fileButton = new QPushButton(tr("发送文件"), connectionBox);
    m_fileButton->setEnabled(false);
    buttons->addWidget(m_hostButton);
    buttons->addWidget(m_joinButton);
    buttons->addWidget(m_disconnectButton);
    buttons->addWidget(m_fileButton);
    form->addRow(buttons);
    sideLayout->addWidget(connectionBox);

    auto *chatTitle = new QLabel(tr("房间聊天"), side);
    m_chatView = new QPlainTextEdit(side);
    m_chatView->setReadOnly(true);
    m_chatView->setMinimumHeight(240);
    auto *chatRow = new QHBoxLayout;
    m_chatEdit = new QLineEdit(side);
    m_chatEdit->setPlaceholderText(tr("输入消息，回车发送"));
    auto *sendButton = new QPushButton(tr("发送"), side);
    chatRow->addWidget(m_chatEdit, 1);
    chatRow->addWidget(sendButton);
    sideLayout->addWidget(chatTitle);
    sideLayout->addWidget(m_chatView, 1);
    sideLayout->addLayout(chatRow);
    m_fileProgress = new QProgressBar(side);
    m_fileProgress->setRange(0, 100);
    m_fileProgress->setVisible(false);
    sideLayout->addWidget(m_fileProgress);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_board);
    splitter->addWidget(side);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({820, 320});
    setCentralWidget(splitter);

    connect(m_hostButton, &QPushButton::clicked, this, &CollabPage::hostRoom);
    connect(m_joinButton, &QPushButton::clicked, this, &CollabPage::joinRoom);
    connect(m_disconnectButton, &QPushButton::clicked, this, [this] {
        emit disconnectRequested();
        if (m_hosting) {
            emit stopServerRequested();
            m_hosting = false;
        }
    });
    connect(sendButton, &QPushButton::clicked, this, &CollabPage::sendChat);
    connect(m_chatEdit, &QLineEdit::returnPressed, this, &CollabPage::sendChat);
    connect(m_fileButton, &QPushButton::clicked, this, &CollabPage::chooseFile);
    statusBar()->showMessage(tr("网络工作线程已启动；创建房间或加入房间"));
}

void CollabPage::hostRoom()
{
    bool ok = false;
    const int port = m_portEdit->text().toInt(&ok);
    if (!ok || port < 0 || port > 65535) {
        statusBar()->showMessage(tr("端口必须在 0 到 65535 之间"), 5000);
        return;
    }
    emit startServerRequested(static_cast<quint16>(port));
}

void CollabPage::joinRoom()
{
    m_hosting = false;
    bool ok = false;
    const int port = m_portEdit->text().toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        statusBar()->showMessage(tr("端口必须在 1 到 65535 之间"), 5000);
        return;
    }
    emit connectRequested(m_hostEdit->text().trimmed(),
                          static_cast<quint16>(port), currentName());
}

void CollabPage::sendChat()
{
    const QString text = m_chatEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    BoardEvent event;
    event.kind = EventKind::Chat;
    event.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event.author = currentName();
    event.text = text;
    if (sendBoardEvent(event)) {
        m_chatEdit->clear();
    }
}

bool CollabPage::sendBoardEvent(const BoardEvent &event)
{
    emit eventSendRequested(event);
    return true;
}

void CollabPage::chooseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("选择要上传到主机的文件"));
    if (!path.isEmpty()) {
        m_fileProgress->setValue(0);
        m_fileProgress->setVisible(true);
        emit fileSendRequested(path);
    }
}

void CollabPage::receiveBoardEvent(const BoardEvent &event)
{
    m_pendingEvents.append(event);
    if (!m_uiFlushTimer->isActive()) {
        m_uiFlushTimer->start();
    }
}

void CollabPage::flushPendingEvents()
{
    const QVector<BoardEvent> events = std::exchange(m_pendingEvents, {});
    bool boardChanged = false;
    for (const BoardEvent &event : events) {
        switch (event.kind) {
        case EventKind::Stroke:
            m_board->applyStroke({event.id, event.author, event.color,
                                  event.width, event.points}, false);
            boardChanged = true;
            break;
        case EventKind::Clear:
            m_board->clearCanvas();
            boardChanged = true;
            break;
        case EventKind::Chat:
            m_chatView->appendPlainText(
                QStringLiteral("[%1] %2：%3")
                    .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                         event.author, event.text));
            break;
        }
    }
    if (boardChanged) {
        m_board->refresh();
    }
}

void CollabPage::setConnectionState(bool connected)
{
    m_hostButton->setEnabled(!connected);
    m_joinButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
    m_fileButton->setEnabled(connected);
    statusBar()->showMessage(connected ? tr("已连接，可以协同") : tr("连接已断开"));
}

QString CollabPage::currentName() const
{
    const QString name = m_nameEdit->text().trimmed();
    return name.isEmpty() ? tr("访客") : name;
}
