#include "timelinewidget.h"
#include "Theme.h"
#include "TimeUtils.h"
#include <QPaintEvent>
#include <QPainter>
#include <QMouseEvent>

namespace {
constexpr int TRACKY = 32;//轨道条顶变
constexpr int TRACKH = 62;//轨道条高度
constexpr int RULERH = 26;//顶部刻度条高度
constexpr int LABELMINWIDTH = 88;//每个至少占多少像素
constexpr double MINSPAN = 30.0;
constexpr double CLICKTOLERANCE = 6;
constexpr int BOTTOMH = 22;
}


TimelineWidget::TimelineWidget(QWidget *parent)
    : QWidget{parent}
{
    setMinimumHeight(140);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    _animation.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&_animation,&QVariantAnimation::valueChanged,this,[this](const QVariant&v){
        const double e = v.toDouble(); // e: 0.0 -> 1.0
        /*_viewStart = _fromS + (_toS - _fromS) * e;
        _viewEnd = _fromE + (_toE - _fromE) *e;
        update();*/
        setView(_fromS + (_toS - _fromS) * e,_fromE+(_toE - _fromE)*e);
    });
}

void TimelineWidget::setDuration(qint64 ms)
{
    _animation.stop();
    _duration = qMax<qint64>(1,ms);
    _viewStart = 0;
    _position = 0;
    _viewEnd = double(_duration);
    update();
}

void TimelineWidget::setPosition(qint64 ms)
{
    _position = ms;
    update();
}

void TimelineWidget::resetView()
{
    animateViewTo(0,double(_duration));
}

void TimelineWidget::setTitle(const QString &t)
{
    _title = t;
    update();
}

void TimelineWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);
    /*背景颜色*/
    p.fillRect(rect(),Theme::BASE);

    drawTrack(p);
    drawRuler(p);
    drawSelection(p);
    drawPlayHead(p);
    drawFooter(p);
}

void TimelineWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() !=Qt::LeftButton){
        QWidget::mousePressEvent(e);
        return;
    }
    _animation.stop();
    _dragging = true;
    _pressPos = _curPos = e->pos();
    update();
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (!_dragging)return;
    _dragging = false;

    const int dx = qAbs(_curPos.x()-_pressPos.x());
    if (dx < CLICKTOLERANCE){
        emit seekRequested(qint64(xToTime(e->pos().x())));
    } else{
        zoomToRange(xToTime(qMin(_pressPos.x(),_curPos.x())),
                xToTime(qMax(_pressPos.x(),_curPos.x())));
    }
    update();
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!_dragging)return;
    _curPos = QPoint(qBound(0,e->pos().x(),width() - 1),e->pos().y());
    update();
}

void TimelineWidget::wheelEvent(QWheelEvent *e)
{
    const double span = _viewEnd - _viewStart;
    if (span < MINSPAN)return;
    const double anchor = xToTime(e->position().x());
    const double k = qBound(0.0,(anchor - _viewStart ) / span,1.0);

    double newSpan = span * ( e->angleDelta().y() > 0 ? 0.75 : 1.33 );
    newSpan = qBound(MINSPAN,newSpan,double(_duration));

    _animation.stop();
    setView(anchor - newSpan*k,anchor+newSpan*(1.0-k));
}

void TimelineWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) { resetView(); return; }
    if (e->key() == Qt::Key_Space)  { emit togglePlayRequested(); return; }
    QWidget::keyPressEvent(e);
}

double TimelineWidget::timeToX(double ms) const
{
    const double span = qMax(1e-9,_viewEnd - _viewStart);
    return (ms - _viewStart) / span * width();
}

double TimelineWidget::xToTime(double x) const
{
    const double span = qMax(1e-9,_viewEnd - _viewStart);
    return _viewStart + x / qMax(1,width()) *span;
}

void TimelineWidget::drawTrack(QPainter &p)
{
    /*轨道底槽*/
    const QRectF track(0,TRACKY,width(),TRACKH);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::TRACKBG);
    p.drawRoundedRect(track,6,6);
    p.setPen(Theme::TEXTMUTED);

    if (!isLoaded())return;
    p.save();
    /*视频条 0~duration映射到像素*/
    const double x0 = timeToX(0);
    const double x1 = timeToX(_duration);
    const QRectF bar(x0,track.top() + 6 , x1-x0,track.height() - 12);

    QLinearGradient g(bar.topLeft(),bar.bottomRight());
    g.setColorAt(0,Theme::CLIPTOP);
    g.setColorAt(1,Theme::CLIPBOTTOM);
    p.setPen(QPen(Theme::CLIPBORDER,1));
    p.setBrush(g);
    p.drawRoundedRect(bar,4,4);
    // p.drawText(track.adjusted(20,0,0,0),Qt::AlignLeft | Qt::AlignVCenter,"轨道底槽");


    const qint64 cell = qMax<qint64>(1,pickStep() / 5);
    p.setPen(QPen(Theme::CELLLINE, 1));
    for(qint64 t = qint64(_viewStart / cell) *cell;t<_viewEnd;t+=cell){
        const double x = timeToX(t);
        p.drawLine(QPointF(x,bar.top() + 4),QPointF(x,bar.bottom() - 4));
    }

    const double played = timeToX(_position) - bar.left();
    if (played > 0){
        p.fillRect(QRectF(bar.left(),bar.top(),played,bar.height()),Theme::PLAYERMASK);
    }

    QFont f = font();
    f.setPointSize(8.0);
    p.setFont(f);
    p.setPen(Theme::TEXTMUTED);
    p.drawText(bar.adjusted(20,0,0,0),Qt::AlignLeft | Qt::AlignVCenter,_title);

    p.restore();
}

void TimelineWidget::drawRuler(QPainter &p)
{
    p.save();

    QFont f = font();
    f.setPointSize(8.0);
    p.setFont(f);

    const qint64 step = pickStep();
    for(qint64 t = qint64(_viewStart / step)*step;t<=_viewEnd+step;t+=step){
        if (t < 0)continue;

        const double x = timeToX(t);
        if (x<-70 || x>width()+70)continue;
        p.setPen(Theme::GRIDLINE);
        p.drawLine(QPointF(x,RULERH),QPointF(x,height()));

        p.setPen(Theme::TEXTDIM);
        p.drawText(QRectF(x+4,0,92,RULERH),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   fmtTime(t,step < 1000));

    }

    p.restore();
}

void TimelineWidget::drawSelection(QPainter &p)
{


    if (!_dragging)return;

    const int xa = qMin(_pressPos.x(),_curPos.x());
    const int xb = qMax(_pressPos.x(),_curPos.x());
    if (xb - xa < 2)return;

    p.save();

    const QRectF selection(xa,RULERH,xb-xa,height() - RULERH-6);
    p.setPen(QPen(QColor(0x4d,0xa3,0xff),1.5));
    p.setBrush(QColor(77,163,255,45));
    p.drawRoundedRect(selection,6,6);

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::ACCENT);
    p.drawRoundedRect(QRectF(xa - 2, selection.top(), 4, selection.height()), 2, 2);
    p.drawRoundedRect(QRectF(xb - 2, selection.top(), 4, selection.height()), 2, 2);

    p.setPen(QColor("#cfe4ff"));
    p.drawText(selection.adjusted(6, 3, -6, -3), Qt::AlignTop | Qt::AlignLeft,
               fmtTime(qint64(xToTime(xa))) + "  →  " + fmtTime(qint64(xToTime(xb))));

    p.restore();
}

void TimelineWidget::drawPlayHead(QPainter &p)
{

    const double px = timeToX(_position);
    if (px<=-10 || px>=width()+10)return;


    p.save();
    p.setPen(QPen(QColor(0xff,0x52,0x52),1.5));
    p.drawLine(QPointF(px,RULERH),QPointF(px,height()));

    QPolygonF tri;
    tri << QPointF(px-6,0) << QPointF(px+6,0) << QPointF(px,RULERH);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xff,0x52,0x52));
    p.drawPolygon(tri);

    p.restore();
}

void TimelineWidget::drawFooter(QPainter &p)
{
    p.setPen(Theme::TEXTFAINT);

    QString info;
    if (isLoaded()) {
        const double zoom = _duration / qMax(1.0, _viewEnd - _viewStart);
        info = QString("可见 %1 → %2    放大 %3x    |    "
                       "拖拽=框选放大   单击=定位   滚轮=缩放   Esc=全览")
                   .arg(fmtTime(qint64(_viewStart), true),
                        fmtTime(qint64(_viewEnd),   true))
                   .arg(zoom, 0, 'f', zoom < 10 ? 2 : (zoom < 100 ? 1 : 0));
    } else {
        info = QStringLiteral("未加载视频 —— 点工具栏「打开视频」");
    }

    p.drawText(QRectF(8, height() - BOTTOMH, width() - 16, BOTTOMH),
               Qt::AlignLeft | Qt::AlignVCenter, info);
}

void TimelineWidget::zoomToRange(double a, double b)
{
    /*_viewStart = a;
    _viewEnd = b;
    update();*/

    animateViewTo(a,b);
}

void TimelineWidget::animateViewTo(double a, double b)
{
    const double total = double(_duration);
    a = qBound(0.0,a,total);
    b=  qBound(0.0,b,total);
    if (b - a < MINSPAN) return; // 太窄，忽略


    _animation.stop();
    _fromS = _viewStart;
    _fromE = _viewEnd;

    _toS = a;
    _toE = b;
    _animation.setStartValue(0.0);
    _animation.setEndValue(1.0);
    _animation.start();
}

/*全局唯一一个可以修改_viewStart/_viewEnd的地方*/
void TimelineWidget::setView(double a, double b)
{
    const double total = double(qMax<qint64>(1,_duration));

    // 太窄
    if (b - a < MINSPAN){
        const double mid = (a+b) *0.5;
        a = mid - MINSPAN*0.5;
        b = mid + MINSPAN*0.5;
    }

    // 贴边
    if (a < 0){b+=a;a=0;}
    if (b>total){a-=(b-total);b=total;}

    // 兜底
    a = qBound(0.0,a,total);
    b = qBound(0.0,b,total);

    _viewStart = a;
    _viewEnd = qMax(a+1,b);
    update();
}

double TimelineWidget::pickStep() const
{
    const double span = qMax(1.0,_viewEnd - _viewStart);
    const double target = span / qMax(1,width() / LABELMINWIDTH);
    static const qint64 cand[] = {
        10,20,50,                           // 毫秒级
        100,200,500,
        1000,2000,5000,                      // 秒级
        10000,15000,30000,
        60000,120000,300000,600000,         // 分级
        900000,1800000,3600000,7200000
    };
    for (qint64 c : cand)
        if (double(c) >= target) return c;

    return 7200000;
}
