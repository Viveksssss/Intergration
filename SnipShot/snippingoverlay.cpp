#include "snippingoverlay.h"

#include "ShotCapture.h"
#include "Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>

namespace {

constexpr int kMinSel    = 8;    // 选区最小边长
constexpr int kHandleHit = 4;    // 手柄的额外命中范围（手感好一点）

}

SnippingOverlay::SnippingOverlay(const QImage &shot,QWidget *parent)
    : QWidget{parent}
    , _shot(shot)
{
    setWindowFlags( Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose,true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);

    setGeometry(ShotCapture::virtualGeometry());
    _mousePos = rect().center();
}

SnippingOverlay::~SnippingOverlay() = default;


void SnippingOverlay::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,false);

    // 1.冻结
    if (!_shot.isNull()){
        p.drawImage(QPoint(0,0),_shot);
    }else{
        p.fillRect(rect(),Qt::black);
    }

    // 2.奇偶规则
    QPainterPath mask;
    mask.setFillRule(Qt::OddEvenFill);
    mask.addRect(QRectF(rect()));
    if (_hasSel){
        mask.addRect(QRectF(_sel));
    }
    p.fillPath(mask,Theme::kMask);


    // 3.选区域装饰
    if (_hasSel){
        paintSelectionChrome(p);
    }

    // 4.十字辅助线条
    paintCrosshair(p);
}

void SnippingOverlay::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pos = e->pos();
    _mousePos = pos;

    if (_mode == Mode::Framing){
        _sel = QRect(_pressPos,pos).normalized().intersected(rect());
        _hasSel = _sel.width() >= 2 && _sel.height() >= 2;
        update();
        return;
    }

    update();
}

void SnippingOverlay::keyPressEvent(QKeyEvent *e)
{
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    switch (e->key()) {
        case Qt::Key_Escape:
            emit cancelled();
            close();
            return;
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down: {
            if (!_hasSel)
                break;
            const int step = shift ? 10 : 1;
            QPoint d;
            if (e->key() == Qt::Key_Left)  d = QPoint(-step, 0);
            if (e->key() == Qt::Key_Right) d = QPoint(step, 0);
            if (e->key() == Qt::Key_Up)    d = QPoint(0, -step);
            if (e->key() == Qt::Key_Down)  d = QPoint(0, step);
            moveSelectionBy(d);
            update();
            return;
        }
        default:
            break;
    }
    QWidget::keyPressEvent(e);
}

void SnippingOverlay::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    setGeometry(ShotCapture::virtualGeometry());
    setFocus();
}

void SnippingOverlay::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton){
        return;
    }
    const QPoint pos =e->pos();
    _mousePos = pos;

    _hasSel = false;
    _mode = Mode::Framing;
    _pressPos = pos;
    _sel = QRect(pos,QSize(0,0));
    update();
}

void SnippingOverlay::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton){
        return;
    }
    if (_mode !=Mode::Framing){
        return;
    }
    _mode = Mode::Idle;
    update();
}

void SnippingOverlay::paintSelectionChrome(QPainter &p)
{
    p.save();

    p.setRenderHint(QPainter::Antialiasing,false);
    p.setBrush(Qt::NoBrush);

    // 白线打底
    p.setPen(QPen(QColor(255,255,255,210),1));
    p.drawRect(_sel.adjusted(0,0,-1,-1));
    p.setPen(QPen(Theme::kSelBorder,1));
    p.drawRect(_sel.adjusted(1,1,-2,-2));

    // 八个手柄
    {
        static const Handle kHandles[] = {
            Handle::TopLeft,Handle::Top, Handle::TopRight, Handle::Right,
            Handle::BottomRight, Handle::Bottom, Handle::BottomLeft, Handle::Left
        };
        p.setPen(QPen(Theme::kSelBorder,1));
        p.setBrush(Theme::kHandleFill);
        for(Handle h : kHandles){
            p.drawRect(handleRect(h));
        }

    }

    // 尺寸标签
    const QString text = QStringLiteral("%1 x %2")
                             .arg(_sel.width())
                             .arg(_sel.height());

    QFont f = Theme::uiFont(8,true);
    f.setPixelSize(12);
    p.setFont(f);

    const QFontMetrics fm(f);
    const int tw = fm.horizontalAdvance(text)+12;
    const int th = fm.height() + 6;

    QRect tag(_sel.left(),_sel.top() - th - 6,tw,th);
    if (tag.top() < 0){
        tag.moveTop(_sel.top()+6);
    }

    p.setRenderHint(QPainter::Antialiasing,true);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::kSizeTagBg);
    p.drawRoundedRect(tag,3,3);
    p.setPen(Qt::white);
    p.drawText(tag,Qt::AlignCenter,text);

    p.restore();
}

void SnippingOverlay::paintCrosshair(QPainter &p)
{
    if (!rect().contains(_mousePos))
        return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(Theme::kCross, 1, Qt::DashLine));
    p.drawLine(rect().left(), _mousePos.y(), rect().right(), _mousePos.y());
    p.drawLine(_mousePos.x(), rect().top(), _mousePos.x(), rect().bottom());
    p.restore();
}

Handle SnippingOverlay::handleAt(const QPoint &pos) const
{
    if (!_hasSel){
        return Handle::None;
    }

    static const Handle kHandles[] = {
        Handle::TopLeft, Handle::Top,    Handle::TopRight, Handle::Right,
        Handle::BottomRight, Handle::Bottom, Handle::BottomLeft, Handle::Left
    };

    for(Handle h : kHandles){
        if (handleRect(h).adjusted(-kHandleHit, -kHandleHit,
                                   kHandleHit, kHandleHit).contains(pos)){
            return h;
        }
    }
    if (_sel.contains(pos)){
        return Handle::Move;
    }
    return Handle::None;
}

QRect SnippingOverlay::handleRect(Handle h) const
{
    const int s = 7;
    QPoint c;
    switch (h) {
    case Handle::TopLeft:     c = QPoint(_sel.left(), _sel.top());           break;
    case Handle::Top:         c = QPoint(_sel.center().x(), _sel.top());     break;
    case Handle::TopRight:    c = QPoint(_sel.right(), _sel.top());          break;
    case Handle::Right:       c = QPoint(_sel.right(), _sel.center().y());   break;
    case Handle::BottomRight: c = QPoint(_sel.right(), _sel.bottom());       break;
    case Handle::Bottom:      c = QPoint(_sel.center().x(), _sel.bottom());  break;
    case Handle::BottomLeft:  c = QPoint(_sel.left(), _sel.bottom());        break;
    case Handle::Left:        c = QPoint(_sel.left(), _sel.center().y());    break;
    case Handle::None:
    case Handle::Move:
        return QRect();
    }
    return QRect(c.x() - s / 2, c.y() - s / 2, s, s);
}

void SnippingOverlay::applyCursorShape(const QPoint &pos)
{
    switch (handleAt(pos)) {
    case Handle::TopLeft:
    case Handle::BottomRight: setCursor(Qt::SizeFDiagCursor); return;
    case Handle::TopRight:
    case Handle::BottomLeft:  setCursor(Qt::SizeBDiagCursor); return;
    case Handle::Left:
    case Handle::Right:       setCursor(Qt::SizeHorCursor);   return;
    case Handle::Top:
    case Handle::Bottom:      setCursor(Qt::SizeVerCursor);   return;
    case Handle::Move:        setCursor(Qt::SizeAllCursor);   return;
    case Handle::None:        break;
    }
    setCursor(Qt::CrossCursor);
}

void SnippingOverlay::moveSelectionBy(const QPoint &d)
{
    QRect r = _sel.translated(d);
    if (r.left() < rect().left())       r.moveLeft(rect().left());
    if (r.top() < rect().top())         r.moveTop(rect().top());
    if (r.right() > rect().right())     r.moveRight(rect().right());
    if (r.bottom() > rect().bottom())   r.moveBottom(rect().bottom());
    const QPoint applied = r.topLeft() - _sel.topLeft();
    if (applied.isNull())
        return;

    _sel = r;
}
