#include "mainwindow.h"
#include "TimeUtils.h"
#include "TimelineWidget.h"

#include <QAudioOutput>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMediaPlayer>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVideoWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}
{
    setWindowTitle("视频查看器");
    resize(1080,720);

    buildUi();
    connectSignals();

    statusBar()->showMessage("拖拽框选一段 → 自动放大到该范围；单击定位；滚轮缩放；Esc 回到全览");
}

void MainWindow::openFile(const QString &path)
{
    _player->setSource(QUrl::fromLocalFile(path));   // 别手拼 "file:///"，中文名会挂
    _timeline->setTitle(QFileInfo(path).fileName());
    setWindowTitle("时间轴框选放大 Demo — " + QFileInfo(path).fileName());
    _player->play();
    statusBar()->showMessage("已加载：" + path);
}

void MainWindow::openVideo()
{
    const QString f = QFileDialog::getOpenFileName(
        this, "选择视频", QString(),
        "视频文件 (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.wmv);;所有文件 (*)");
    if (!f.isEmpty()) openFile(f);
}

void MainWindow::togglePlay()
{
    if (!_player->source().isValid()) {
        statusBar()->showMessage("还没有打开视频");
        return;
    }
    if (_player->playbackState() == QMediaPlayer::PlayingState) _player->pause();
    else                                                        _player->play();
}

void MainWindow::buildUi()
{
    _player = new QMediaPlayer(this);
    _audio = new QAudioOutput(this);
    _player->setAudioOutput(_audio);

    _video = new QVideoWidget(this);
    _video->setMinimumSize(320,180);
    _video->setAutoFillBackground(true);

    {
        QPalette vp = _video->palette();
        vp.setColor(QPalette::Window,Qt::black);
        _video->setPalette(vp);
    }

    _player->setVideoOutput(_video);


    _timeline = new TimelineWidget;
    auto*split = new QSplitter(Qt::Vertical);
    split->addWidget(_video);
    split->addWidget(_timeline);
    split->setStretchFactor(0,5);
    split->setStretchFactor(1,2);
    split->setCollapsible(0,false);
    split->setCollapsible(1,false);
    setCentralWidget(split);


    auto*tb = addToolBar("main");
    tb->setMovable(true);
    tb->addAction("Open Video",this,&MainWindow::openVideo);
    tb->addSeparator();

    _playAct = tb->addAction("Play",this,&MainWindow::togglePlay);
    tb->addAction("Reset",_timeline,&TimelineWidget::resetView);
    tb->addSeparator();

    _timeLab = new QLabel("  0:00 / 0:00  ");
    tb->addWidget(_timeLab);
}

void MainWindow::connectSignals()
{
    connect(_player,&QMediaPlayer::durationChanged,_timeline,&TimelineWidget::setDuration);
    connect(_player,&QMediaPlayer::positionChanged,_timeline,&TimelineWidget::setPosition);
    connect(_timeline,&TimelineWidget::seekRequested,_player,&QMediaPlayer::setPosition);

    connect(_timeline,&TimelineWidget::togglePlayRequested,this,&MainWindow::togglePlay);
    connect(_player,&QMediaPlayer::positionChanged,this,[this](qint64 p){
        _timeLab->setText(QString("  %1 / %2  ").arg(fmtTime(p)).arg(fmtTime(_player->duration())));
    });

    connect(_player,&QMediaPlayer::playbackStateChanged,this,[this](QMediaPlayer::PlaybackState s){
        _playAct->setText(s==QMediaPlayer::PlayingState ? "暂停" :"播放");
    });

    connect(_player,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString&msg){
        this->statusBar()->showMessage("播放出错:"+msg);
    });

    this->statusBar()->showMessage("sdadasd");
}
