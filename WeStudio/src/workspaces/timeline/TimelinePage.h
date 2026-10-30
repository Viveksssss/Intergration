// ============================================================================
//  TimelinePage.h —— 主窗口
// ----------------------------------------------------------------------------
//  职责很单一：把「播放器」和「时间轴」粘在一起。
//  它不画任何东西，也不做时间换算 —— 那些都在 TimelineWidget 里。
//  这就是分文件的意义：每个文件只回答一个问题。
// ============================================================================
#pragma once

#include <QMainWindow>

class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class QLabel;
class QAction;
class TimelineWidget;

class TimelinePage : public QMainWindow
{
    Q_OBJECT

public:
    explicit TimelinePage(QWidget *parent = nullptr);

    void openFile(const QString &path);   // 供命令行/拖拽调用
    void loadDemo();                      // 没视频时也能看效果

private slots:
    void openVideo();                     // 弹文件对话框
    void togglePlay();

private:
    void buildUi();                       // 建控件 + 布局 + 工具栏
    void connectSignals();                // 所有 connect 集中在一处

    QMediaPlayer   *m_player   = nullptr;
    QAudioOutput   *m_audio    = nullptr;
    QVideoWidget   *m_video    = nullptr;
    TimelineWidget *m_timeline = nullptr;
    QAction        *m_playAct  = nullptr;   // 「播放/暂停」按钮，需要改文字
    QLabel         *m_timeLab  = nullptr;   // 工具栏右侧的 0:00 / 0:00
};
