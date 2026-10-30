#include "TimelinePage.h"

#include "TimelineTheme.h"
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
#include <QVBoxLayout>
#include <QVideoWidget>

TimelinePage::TimelinePage(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("时间轴框选放大 Demo");
    resize(1100, 720);

    buildUi();
    connectSignals();

    statusBar()->showMessage("拖拽框选一段 → 自动放大到该范围；单击定位；滚轮缩放；Esc 回到全览");
}

void TimelinePage::buildUi()
{
    // ---------------- 播放器 ----------------
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_player->setAudioOutput(m_audio);   // Qt6 必须显式给音频输出，否则没声音

    m_video = new QVideoWidget;
    m_video->setMinimumSize(320, 180);
    m_video->setAutoFillBackground(true);
    {
        // QVideoWidget 自己不清背景，不设成黑色的话未播放时是花的
        QPalette vp = m_video->palette();
        vp.setColor(QPalette::Window, Qt::black);
        m_video->setPalette(vp);
    }
    m_player->setVideoOutput(m_video);

    // ---------------- 时间轴 ----------------
    m_timeline = new TimelineWidget;

    auto *split = new QSplitter(Qt::Vertical);
    split->addWidget(m_video);
    split->addWidget(m_timeline);
    split->setStretchFactor(0, 3);          // 视频区 : 时间轴 = 3 : 2
    split->setStretchFactor(1, 2);
    split->setCollapsible(0, false);        // 不允许把某一边拖到 0
    split->setCollapsible(1, false);
    setCentralWidget(split);

    // ---------------- 工具栏 ----------------
    auto *tb = addToolBar("main");
    tb->setMovable(false);
    tb->addAction("打开视频", this, &TimelinePage::openVideo);
    tb->addAction("演示数据", this, &TimelinePage::loadDemo);
    tb->addSeparator();

    m_playAct = tb->addAction("播放", this, &TimelinePage::togglePlay);
    tb->addAction("复位缩放", m_timeline, &TimelineWidget::resetView);  // 直接连到控件的槽
    tb->addSeparator();

    m_timeLab = new QLabel("  0:00 / 0:00  ");
    tb->addWidget(m_timeLab);
}

void TimelinePage::connectSignals()
{
    // ================== 三条主线 ==================
    // 下行 1：总时长就绪 -> 时间轴铺满全览
    connect(m_player, &QMediaPlayer::durationChanged,
            m_timeline, &TimelineWidget::setDuration);

    // 下行 2：播放位置变化（约 50ms 一次）-> 播放头移动
    connect(m_player, &QMediaPlayer::positionChanged,
            m_timeline, &TimelineWidget::setPosition);

    // 上行 1：用户在时间轴上单击 -> 让播放器跳转
    connect(m_timeline, &TimelineWidget::seekRequested,
            m_player, &QMediaPlayer::setPosition);

    // ================== 附属连接 ==================
    connect(m_timeline, &TimelineWidget::togglePlayRequested,
            this, &TimelinePage::togglePlay);

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 p) {
        m_timeLab->setText(QString("  %1 / %2  ")
                           .arg(fmtTime(p), fmtTime(m_player->duration())));
    });

    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState s) {
                m_playAct->setText(s == QMediaPlayer::PlayingState ? "暂停" : "播放");
            });

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &msg) {
                statusBar()->showMessage("播放出错：" + msg);
            });
}

void TimelinePage::openVideo()
{
    const QString f = QFileDialog::getOpenFileName(
        this, "选择视频", QString(),
        "视频文件 (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.wmv);;所有文件 (*)");
    if (!f.isEmpty()) openFile(f);
}

void TimelinePage::openFile(const QString &path)
{
    m_player->setSource(QUrl::fromLocalFile(path));   // 别手拼 "file:///"，中文名会挂
    m_timeline->setTitle(QFileInfo(path).fileName());
    setWindowTitle("时间轴框选放大 Demo — " + QFileInfo(path).fileName());
    m_player->play();
    statusBar()->showMessage("已加载：" + path);
}

void TimelinePage::loadDemo()
{
    m_player->stop();
    m_player->setSource(QUrl());
    setWindowTitle("时间轴框选放大 Demo — 演示数据");
    m_timeline->setTitle("演示轨道（5 分钟）");
    m_timeline->setDuration(5 * 60 * 1000);
    statusBar()->showMessage("已载入 5:00 演示轨道，拖一段试试");
}

void TimelinePage::togglePlay()
{
    if (!m_player->source().isValid()) {
        statusBar()->showMessage("还没有打开视频");
        return;
    }
    if (m_player->playbackState() == QMediaPlayer::PlayingState) m_player->pause();
    else                                                        m_player->play();
}
