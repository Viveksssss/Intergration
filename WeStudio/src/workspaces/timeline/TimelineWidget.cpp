// ============================================================================
//  TimelineWidget.cpp —— 时间轴控件的实现
// ----------------------------------------------------------------------------
//  阅读顺序建议（从「概念」到「细节」）：
//      ① 构造        控件初始化
//      ② 映射        timeToX / xToTime   ← 全文件最重要 8 行
//      ③ 视口        setView / animateViewTo / zoomToRange / resetView
//      ④ 刻度        pickStep
//      ⑤ 绘制        paintEvent + 5 个 drawXxx
//      ⑥ 交互        鼠标 / 滚轮 / 键盘
// ============================================================================
#include "TimelineWidget.h"

#include "TimelineTheme.h"
#include "TimeUtils.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

namespace {

// ---- 布局常量：只在本文件用，所以放匿名 namespace（等价于 static）----
constexpr int kRulerH  = 26;   // 顶部刻度条高度
constexpr int kTrackY  = 32;   // 轨道条顶边 y
constexpr int kTrackH  = 62;   // 轨道条高度
constexpr int kBottomH = 22;   // 底部信息条高度

constexpr int    kClickTolerance = 6;     // 位移小于 6px 算单击，否则算框选
constexpr double kMinSpan        = 30.0;  // 最小可见跨度(ms)，防无限放大把浮点榨干
constexpr int    kLabelMinWidth  = 88;    // 每个刻度格至少占多少像素

}  // namespace

// ============================================================================
//  ① 构造
// ============================================================================
TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(140);
    setCursor(Qt::CrossCursor);

    // 关键一行：没有它，keyPressEvent 永远不会被调用（Esc / 空格全失效），
    // 而且编译器不会给任何提示。新手第一大坑。
    setFocusPolicy(Qt::StrongFocus);

    // 缩放动画：只驱动一个 0→1 的进度，真正的视口插值在回调里做
    m_anim.setDuration(420);
    m_anim.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const double e = v.toDouble();                 // e: 0.0 → 1.0
        setView(m_fromS + (m_toS - m_fromS) * e,       // 起点独立插值
                m_fromE + (m_toE - m_fromE) * e);      // 终点独立插值
    });
}

// ============================================================================
//  ② 映射：时间 <-> 像素
// ----------------------------------------------------------------------------
//  整个工程的"放大"效果就藏在这 8 行里。把边界值代进去看：
//      timeToX(m_viewStart) == 0                -> 窗口起点永远贴屏幕最左
//      timeToX(m_viewEnd)   == width()          -> 窗口终点永远贴屏幕最右
//  也就是说：窗口里装的内容，永远铺满控件。所以把窗口从 [0, 300000] 换成
//  [60000, 150000]，"放大"这件事自己就发生了 —— 不需要任何缩放代码。
// ============================================================================
double TimelineWidget::timeToX(double ms) const
{
    const double span = qMax(1e-9, m_viewEnd - m_viewStart);   // 防 0 除
    return (ms - m_viewStart) / span * width();
}

double TimelineWidget::xToTime(double x) const
{
    const double span = qMax(1e-9, m_viewEnd - m_viewStart);
    return m_viewStart + x / qMax(1, width()) * span;
}

// ============================================================================
//  ③ 视口
// ============================================================================

// 全程序唯一能修改 m_viewStart / m_viewEnd 的地方。
// 只要所有路径（框选、滚轮、动画回调、复位）都走这里，就不可能出现非法视口。
void TimelineWidget::setView(double a, double b)
{
    const double total = double(qMax<qint64>(1, m_duration));

    // 1) 太窄：以中点为准撑到 kMinSpan。
    //    不做的话，连续滚轮放大 20 次后 span 会退化成 1e-12 量级，
    //    刻度乱跳、轨道抽搐 —— 那是浮点精度问题，不是逻辑 bug。
    if (b - a < kMinSpan) {
        const double mid = (a + b) * 0.5;
        a = mid - kMinSpan * 0.5;
        b = mid + kMinSpan * 0.5;
    }

    // 2) 贴边：撞到边界时「整体平移」，保持跨度不变。
    //    如果只把越界那端截断，跨度会突变，动画看起来像被抽了一下。
    if (a < 0)     { b -= a;              a = 0; }
    if (b > total) { a -= (b - total);    b = total; }

    a = qBound(0.0, a, total);
    b = qBound(0.0, b, total);

    m_viewStart = a;
    m_viewEnd   = qMax(b, a + 1.0);   // 兜底：至少 1ms
    update();
}

// 带动画：从当前视口平滑过渡到 [a,b]
void TimelineWidget::animateViewTo(double a, double b)
{
    if (!isLoaded()) return;

    const double total = double(m_duration);
    a = qBound(0.0, a, total);
    b = qBound(0.0, b, total);
    if (b - a < kMinSpan) return;      // 选得太窄，放大没意义，直接忽略

    m_anim.stop();                     // 打断上一次动画，避免两个动画打架
    m_fromS = m_viewStart;  m_fromE = m_viewEnd;
    m_toS   = a;            m_toE   = b;
    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.start();
}

void TimelineWidget::setDuration(qint64 ms)
{
    m_anim.stop();
    m_duration  = qMax<qint64>(0, ms);
    m_position  = 0;
    m_viewStart = 0;                                  // 新视频一律先给全览
    m_viewEnd   = double(qMax<qint64>(1, m_duration));
    update();
}

void TimelineWidget::setPosition(qint64 ms)
{
    m_position = ms;
    update();                          // 播放时每 50ms 调一次，播放头就动起来了
}

void TimelineWidget::setTitle(const QString &title)
{
    m_title = title;
    update();
}

void TimelineWidget::zoomToRange(double a, double b)
{
    animateViewTo(a, b);
}

void TimelineWidget::resetView()
{
    animateViewTo(0, double(m_duration));
}

// ============================================================================
//  ④ 刻度步长
// ----------------------------------------------------------------------------
//  不能写死"1 秒一格"：放大到毫秒级会挤成一团，缩小到 2 小时会只剩几条线。
//  做法是：先算出"为了每格至少 88px，每个格子应该是多少毫秒"（target），
//  再从候选表里挑第一个 >= target 的值。这样刻度永远是"人类习惯"的整数。
// ============================================================================
qint64 TimelineWidget::pickStep() const
{
    const double span   = qMax(1.0, m_viewEnd - m_viewStart);
    const double target = span / qMax(1, width() / kLabelMinWidth);

    static const qint64 cand[] = {
        10, 20, 50,                          // 毫秒级（放大到极限时用）
        100, 200, 500,
        1000, 2000, 5000,                    // 秒级
        10000, 15000, 30000,
        60000, 120000, 300000, 600000,       // 分级
        900000, 1800000, 3600000, 7200000
    };
    for (qint64 c : cand)
        if (double(c) >= target) return c;
    return 7200000;
}

// ============================================================================
//  ⑤ 绘制
// ----------------------------------------------------------------------------
//  paintEvent 本身不含任何几何计算，只是按图层顺序调用 5 个函数。
//  想调外观，直接翻到对应的 drawXxx，不用在 100 行里找。
// ============================================================================
void TimelineWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), TimelineTheme::kBase);     // 画布底色

    drawTrack(p);        // 最底层：轨道 + 视频条
    drawRuler(p);        // 刻度压在轨道上面
    drawSelection(p);    // 框选框
    drawPlayhead(p);     // 播放头
    drawFooter(p);       // 底部文字
}

void TimelineWidget::drawTrack(QPainter &p)
{
    const QRectF track(0, kTrackY, width(), kTrackH);

    p.setPen(Qt::NoPen);
    p.setBrush(TimelineTheme::kTrackBg);
    p.drawRoundedRect(track, 6, 6);

    if (!isLoaded()) return;

    // 裁剪到轨道范围内：放大后视频条会远远超出屏幕，不裁会盖住刻度区
    p.save();
    p.setClipRect(track);

    const double x0 = timeToX(0);
    const double x1 = timeToX(m_duration);
    const QRectF bar(x0, track.top() + 6, x1 - x0, track.height() - 12);

    QLinearGradient g(bar.topLeft(), bar.bottomLeft());
    g.setColorAt(0, TimelineTheme::kClipTop);
    g.setColorAt(1, TimelineTheme::kClipBottom);
    p.setPen(QPen(TimelineTheme::kClipBorder, 1));
    p.setBrush(g);
    p.drawRoundedRect(bar, 4, 4);

    // 帧格：按刻度步长的 1/5 分格（所以视觉上是"4 短 1 长"）
    const qint64 cell = qMax<qint64>(1, pickStep() / 5);
    p.setPen(QPen(TimelineTheme::kCellLine, 1));
    for (qint64 t = qint64(m_viewStart / cell) * cell; t < m_viewEnd; t += cell) {
        const double x = timeToX(t);
        p.drawLine(QPointF(x, bar.top() + 4), QPointF(x, bar.bottom() - 4));
    }

    // 已播放部分高亮
    const double played = timeToX(m_position) - bar.left();
    if (played > 0)
        p.fillRect(QRectF(bar.left(), bar.top(), played, bar.height()), TimelineTheme::kPlayedMask);

    // 轨道标题（放大到条很宽时会被裁到屏幕外，无所谓）
    QFont f = font();
    f.setPointSizeF(8.0);
    p.setFont(f);
    p.setPen(TimelineTheme::kTextMuted);
    p.drawText(bar.adjusted(8, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    p.restore();
}

void TimelineWidget::drawRuler(QPainter &p)
{
    QFont f = font();
    f.setPointSizeF(8.0);
    p.setFont(f);

    const qint64 step = pickStep();
    // 起点向下取整到 step 的整数倍：保证刻度线是"绝对时间"而不是随视口漂移
    for (qint64 t = qint64(m_viewStart / step) * step; t <= m_viewEnd + step; t += step) {
        if (t < 0) continue;
        const double x = timeToX(t);
        if (x < -70 || x > width() + 70) continue;   // 屏幕外的不画，省点力气

        p.setPen(TimelineTheme::kGridLine);
        p.drawLine(QPointF(x, kRulerH), QPointF(x, height() - kBottomH));

        p.setPen(TimelineTheme::kTextDim);
        p.drawText(QRectF(x + 4, 0, 92, kRulerH),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   fmtTime(t, step < 1000));          // 步长小于 1 秒才显示毫秒
    }
}

void TimelineWidget::drawSelection(QPainter &p)
{
    if (!m_dragging) return;

    const int xa = qMin(m_pressPos.x(), m_curPos.x());
    const int xb = qMax(m_pressPos.x(), m_curPos.x());
    if (xb - xa < 2) return;                          // 2px 以下不画，避免抖动

    const QRectF sel(xa, kRulerH, xb - xa, height() - kBottomH - kRulerH);

    p.setPen(QPen(TimelineTheme::kAccent, 1.5));
    p.setBrush(TimelineTheme::kSelFill);
    p.drawRect(sel);

    // 两端的把手
    p.setPen(Qt::NoPen);
    p.setBrush(TimelineTheme::kAccent);
    p.drawRoundedRect(QRectF(xa - 2, sel.top(), 4, sel.height()), 2, 2);
    p.drawRoundedRect(QRectF(xb - 2, sel.top(), 4, sel.height()), 2, 2);

    // 框里的起止时间
    p.setPen(QColor("#cfe4ff"));
    p.drawText(sel.adjusted(6, 3, -6, -3), Qt::AlignTop | Qt::AlignLeft,
               fmtTime(qint64(xToTime(xa))) + "  →  " + fmtTime(qint64(xToTime(xb))));
}

void TimelineWidget::drawPlayhead(QPainter &p)
{
    if (!isLoaded()) return;

    const double px = timeToX(m_position);
    if (px <= -10 || px >= width() + 10) return;      // 移出视野就不画

    p.setPen(QPen(TimelineTheme::kPlayhead, 1.5));
    p.drawLine(QPointF(px, kRulerH), QPointF(px, height() - kBottomH));

    // 顶部的小三角
    QPolygonF tri;
    tri << QPointF(px - 6, 0) << QPointF(px + 6, 0) << QPointF(px, kRulerH);
    p.setPen(Qt::NoPen);
    p.setBrush(TimelineTheme::kPlayhead);
    p.drawPolygon(tri);
}

void TimelineWidget::drawFooter(QPainter &p)
{
    p.setPen(TimelineTheme::kTextFaint);

    QString info;
    if (isLoaded()) {
        const double zoom = m_duration / qMax(1.0, m_viewEnd - m_viewStart);
        info = QString("可见 %1 → %2    放大 %3x    |    "
                       "拖拽=框选放大   单击=定位   滚轮=缩放   Esc=全览")
                   .arg(fmtTime(qint64(m_viewStart), true),
                        fmtTime(qint64(m_viewEnd),   true))
                   .arg(zoom, 0, 'f', zoom < 10 ? 2 : (zoom < 100 ? 1 : 0));
    } else {
        info = QStringLiteral("未加载视频 —— 点工具栏「打开视频」，或点「演示数据」先看效果");
    }

    p.drawText(QRectF(8, height() - kBottomH, width() - 16, kBottomH),
               Qt::AlignLeft | Qt::AlignVCenter, info);
}

// ============================================================================
//  ⑥ 交互
// ============================================================================

void TimelineWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }

    m_anim.stop();                    // 用户一动手就打断动画
    m_dragging = true;
    m_pressPos = m_curPos = e->pos();
    update();
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragging) return;
    // 横向钳在控件内，否则拖到窗口外再回来，x 会变成负数
    m_curPos = QPoint(qBound(0, e->pos().x(), width() - 1), e->pos().y());
    update();
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (!m_dragging) return;
    m_dragging = false;

    const int dx = qAbs(m_curPos.x() - m_pressPos.x());

    if (dx < kClickTolerance) {
        // 位移很小 -> 当成单击：定位播放头
        // （不区分的话，用户想点一下定位，实际有大概率变成"框选了 2 像素"然后放大 500 倍）
        emit seekRequested(qint64(xToTime(e->pos().x())));
    } else {
        // 拖拽 -> 把选中的时间区间变成新的可见窗口 = 放大
        zoomToRange(xToTime(qMin(m_pressPos.x(), m_curPos.x())),
                    xToTime(qMax(m_pressPos.x(), m_curPos.x())));
    }
    update();
}

void TimelineWidget::wheelEvent(QWheelEvent *e)
{
    if (!isLoaded()) return;

    const double span   = m_viewEnd - m_viewStart;
    const double anchor = xToTime(e->position().x());       // 鼠标下的那个时间点
    const double k      = qBound(0.0, (anchor - m_viewStart) / span, 1.0);  // 它在窗口里的相对位置

    double newSpan = span * (e->angleDelta().y() > 0 ? 0.75 : 1.3333);      // 上滚放大
    newSpan = qBound(kMinSpan, newSpan, double(m_duration));

    m_anim.stop();
    // 让 anchor 在窗口里的相对位置 k 保持不变 —— 这就是"以鼠标为中心缩放"
    setView(anchor - newSpan * k, anchor + newSpan * (1.0 - k));
}

void TimelineWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) { resetView();              return; }
    if (e->key() == Qt::Key_Space)  { emit togglePlayRequested(); return; }
    QWidget::keyPressEvent(e);
}
