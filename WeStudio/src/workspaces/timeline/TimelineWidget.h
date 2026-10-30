// ============================================================================
//  TimelineWidget.h —— 时间轴控件的「接口说明书」
// ----------------------------------------------------------------------------
//  这个头文件只回答三件事：
//    1. 外面能怎么用它        （public / slots / signals）
//    2. 它拦了哪些系统事件    （protected 的 event 覆写）
//    3. 它内部有哪些状态      （private 的成员变量）
//
//  真正的实现全在 TimelineWidget.cpp，读这个头文件就能知道它怎么跟外界打交道，
//  不用先读 300 行绘制代码。
// ============================================================================
#pragma once

#include <QPoint>
#include <QString>
#include <QVariantAnimation>
#include <QWidget>

class QPainter;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

class TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget *parent = nullptr);

    // ---- 外界喂数据（通常来自 QMediaPlayer）----
    void setDuration(qint64 ms);          // 视频总时长，同时把视口复位成全览
    void setPosition(qint64 ms);          // 当前播放位置 -> 播放头
    void setTitle(const QString &title);  // 轨道条上显示的文字

    bool isLoaded() const { return m_duration > 0; }

public slots:
    void resetView();                     // 带动画回到全览（Esc / 工具栏按钮）
    void zoomToRange(double a, double b); // 带动画放大到 [a,b] 毫秒

signals:
    void seekRequested(qint64 ms);        // 单击定位 -> 请播放器跳转
    void togglePlayRequested();           // 空格 -> 请主窗口播放/暂停

protected:
    // 这里只列出"我改写了"，具体怎么写全在 .cpp
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    // ---- 绘制：拆成 5 个函数，paintEvent 只负责按顺序调用 ----
    void drawTrack(QPainter &p);      // 底槽 + 视频条 + 帧格 + 已播放高亮 + 标题
    void drawRuler(QPainter &p);      // 刻度线 + 时间文字
    void drawSelection(QPainter &p);  // 框选矩形
    void drawPlayhead(QPainter &p);   // 红色播放头
    void drawFooter(QPainter &p);     // 底部状态文字

    // ---- 坐标映射（整个效果的地基，只有这两个函数）----
    double timeToX(double ms) const;  // 时间 -> 像素
    double xToTime(double x) const;   // 像素 -> 时间

    // ---- 视口 ----
    void setView(double a, double b);          // 立即生效，含全部边界钳制
    void animateViewTo(double a, double b);    // 从当前视口平滑过渡到 [a,b]

    qint64 pickStep() const;                   // 按当前跨度挑一个合适的刻度步长

    // ---- 状态：整个控件就这么多 ----
    double  m_viewStart = 0;      // 可见窗口起点(ms)   ← 核心状态
    double  m_viewEnd   = 1;      // 可见窗口终点(ms)   ← 核心状态
    qint64  m_duration  = 0;      // 视频总时长
    qint64  m_position  = 0;      // 播放头位置
    QString m_title;

    bool    m_dragging = false;   // 是否正在框选
    QPoint  m_pressPos;           // 按下的位置
    QPoint  m_curPos;             // 当前拖到的位置

    QVariantAnimation m_anim;     // 缩放动画：只吐 0→1 的进度
    double  m_fromS = 0, m_fromE = 1;   // 动画起点（视口）
    double  m_toS   = 0, m_toE   = 1;   // 动画终点（视口）
};
