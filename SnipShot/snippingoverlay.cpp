#include "snippingoverlay.h"

#include "ShotCapture.h"
#include "Theme.h"
#include "toolbar.h"
#include <QStandardPaths>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QStandardPaths>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

namespace {

constexpr int kMinSel    = 8;    // 选区最小边长
constexpr int kHandleHit = 4;    // 手柄的额外命中范围（手感好一点）

QPainterPath smoothPath(const QVector<QPointF>&pts){
    QPainterPath path;
    if (pts.empty()){
        return path;
    }
    if (pts.size() == 1){
        path.moveTo(pts[0]);
        path.lineTo(pts[0] + QPointF(0.1, 0.1));
        return path;
    }
    path.moveTo(pts[0]);
    if (pts.size() == 2){
        path.lineTo(pts[1]);
        return path;
    }

    for(int i = 1; i < pts.size() - 1;++i){
        const QPointF mid = (pts[i]+pts[i+1]) * 0.5;
        path.quadTo(pts[i],mid);
    }
    path.lineTo(pts.last());
    return path;
}

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
    _color = Theme::kInkRed;

    _bar = new ToolBar(this);
    _bar->hide();


    connect(_bar,&ToolBar::toolChanged,this,[this](Tool t){
        selectTool(t);
        setFocus();
    });
    connect(_bar, &ToolBar::undoRequested, this, [this] {
        undo();
        setFocus();
    });
    connect(_bar, &ToolBar::clearRequested, this, [this] {
        clearAnnotations();
        setFocus();
    });
    connect(_bar, &ToolBar::copyRequested, this, &SnippingOverlay::finishAndCopy);
    connect(_bar, &ToolBar::saveRequested, this, &SnippingOverlay::saveToFile);
    connect(_bar, &ToolBar::cancelRequested, this, [this] {
        emit cancelled();
        close();
    });

}

QImage SnippingOverlay::result() const
{
    if (!_hasSel)return QImage();
    return ShotCapture::renderRegion(_shot,_sel,_anns);
}

void SnippingOverlay::selectTool(Tool t)
{
    _tool = t;
    applyCursorShape(_mousePos);
    update();
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

    // 3.标注
    if (_hasSel){
        p.save();
        p.setClipRect(_sel);
        for(const Annotation&a : _anns){
            ShotCapture::paintOne(p,_shot,a);
        }
        if (_hasDraft){
            ShotCapture::paintOne(p,_shot,_draft);
        }
        p.restore();
    }


    // 4.选区域装饰
    if (_hasSel){
        paintSelectionChrome(p);
    }

    // 5.十字辅助线条
    paintCrosshair(p);
}

void SnippingOverlay::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pos = e->pos();
    _mousePos = pos;

    switch (_mode){
    case Mode::Drawing:{
        updateDraft(pos);
        update();
        return;
    }
    case Mode::Idle:{
        applyCursorShape(pos);
        update();
        return;
    }
    case Mode::Framing:{
        _sel = QRect(_pressPos,pos).normalized().intersected(rect());
        _hasSel = _sel.width() >= 2 && _sel.height() >= 2;
        update();
        return;
    }
    case Mode::Moving:{
        const QPoint d = pos - _lastPos;
        if (!d.isNull())moveSelectionBy(d);
        _lastPos = pos;
        layoutToolBar();
        update();
        return;
    }
    case Mode::Resizing:{
        QRect r = _pressSel;
        switch (_handle){
        case Handle::Left:
        case Handle::TopLeft:
        case Handle::BottomLeft:  r.setLeft(pos.x());   break;
        case Handle::Right:
        case Handle::TopRight:
        case Handle::BottomRight: r.setRight(pos.x());  break;
        default: break;
        }

        switch (_handle) {
        case Handle::Top:
        case Handle::TopLeft:
        case Handle::TopRight:    r.setTop(pos.y());    break;
        case Handle::Bottom:
        case Handle::BottomLeft:
        case Handle::BottomRight: r.setBottom(pos.y()); break;
        default: break;
        }
        r = r.normalized().intersected(rect());
        if (r.width() >= kMinSel && r.height() >= kMinSel){
            _sel = r;
        }
        update();
        return;
        }
    }

    update();
}

void SnippingOverlay::keyPressEvent(QKeyEvent *e)
{
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    const bool ctrl  = e->modifiers() & Qt::ControlModifier;
    switch (e->key()) {
        case Qt::Key_Escape:
            emit cancelled();
            close();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            finishAndCopy();
            return;

        case Qt::Key_Z:
            if (ctrl) {
                shift ? redo() : undo();
                return;
            }
            break;

        case Qt::Key_Y:
            if (ctrl) {
                redo();
                return;
            }
            break;

        case Qt::Key_C:
            if (ctrl) {
                finishAndCopy();
                return;
            }
            break;

        case Qt::Key_S:
            if (ctrl) {
                saveToFile();
                return;
            }
            break;

        case Qt::Key_Delete:
            clearAnnotations();
            return;
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down: {
            if (!_hasSel)
                break;
            const int step = shift ? 10 : 1;
            QPointF d;
            if (e->key() == Qt::Key_Left)  d = QPointF(-step, 0);
            if (e->key() == Qt::Key_Right) d = QPointF(step, 0);
            if (e->key() == Qt::Key_Up)    d = QPointF(0, -step);
            if (e->key() == Qt::Key_Down)  d = QPointF(0, step);
            moveSelectionBy(d);
            update();
            return;
        }
        default:
            break;
    }

    if (!ctrl && !(e->modifiers() & Qt::AltModifier)) {
        switch (e->key()) {
        case Qt::Key_R: selectTool(Tool::Rect);   return;
        case Qt::Key_A: selectTool(Tool::Arrow);  return;
        case Qt::Key_P: selectTool(Tool::Pen);    return;
        case Qt::Key_T: selectTool(Tool::Text);   return;
        case Qt::Key_M: selectTool(Tool::Mosaic); return;
        case Qt::Key_C: selectTool(Tool::None);   return;
        default: break;
        }
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
    _lastPos = pos;

    if (_textEdit){
        commitText();
    }

    // 工具激活+按在选区->绘制
    if (_tool != Tool::None && _hasSel &&  _sel.contains(pos)){
        if (_tool == Tool::Text){
            beginTextInput(pos);
            return;
        }
        _pressPos = pos;

        pushUndo();
        _draft = Annotation();
        _draft.tool = _tool;
        _draft.color = _color;
        _draft.width = _penWidth;
        _draft.block = _block;
        _draft.p1 = pos;
        _draft.p2 = pos;
        _draft.rect = QRectF(pos,QSizeF(0,0));

        if (_tool == Tool::Pen) {
            _penPoints.clear();
            _penPoints << pos;
        }

        _hasDraft = true;
        _mode = Mode::Drawing;
        update();
        return;
    }

    // 按在手柄
    const Handle h = handleAt(pos);
    if (h!=Handle::None){
        _handle = h;
        _mode = (h == Handle::Move) ? Mode::Moving : Mode::Resizing;
        _pressSel = _sel;
        return;
    }

    // 其他地方
    _hasSel = false;
    _anns.clear();
    _undo.clear();
    _redo.clear();

    _hasDraft = false;
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

    if (_mode == Mode::Drawing){

        if (_tool == Tool::Pen){
            _draft.path = smoothPath(_penPoints);
        }

        if (_draft.isEmpty()){
            popUndo();
        }else{
            _anns.append(_draft);
            _bar->setUndoEnabled(true);
        }
        _hasDraft = false;
        _mode = Mode::Idle;
        applyCursorShape(e->pos());
        update();
        return;
    }
    if (_mode ==Mode::Framing){
        _mode = Mode::Idle;
        layoutToolBar();
        applyCursorShape(e->pos());
        update();
        return;
    }
    _mode = Mode::Idle;
    _handle = Handle::None;
    applyCursorShape(e->pos());
    update();
}

void SnippingOverlay::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (_tool == Tool::None && _hasSel && _sel.contains((e->pos()))){
        finishAndCopy();
    }
}

void SnippingOverlay::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutToolBar();
}

void SnippingOverlay::layoutToolBar()
{
    if (!_bar){
        return;
    }
    if (!_hasSel){
        _bar->hide();
        return;
    }
    _bar->placeNear(_sel,rect());
    _bar->show();
    _bar->raise();
}

void SnippingOverlay::finishAndCopy()
{
    const QImage img = result();
    if (img.isNull()){
        return;
    }
    emit finished(img);
    close();
}

void SnippingOverlay::saveToFile()
{
    const QImage img = result();
    if (img.isNull()){
        return;
    }

    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty()){
        dir = QDir::homePath();
    }

    const QString suggested = QStringLiteral("%1/Snip_%2.png")
                                  .arg(dir)
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this,QStringLiteral("保存截图"),suggested,QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg)"));
    if (path.isEmpty()){
        return;
    }
    if (!img.save(path)){
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入：\n%1").arg(path));
        return;
    }
    emit finished(img);
    close();
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

Handle SnippingOverlay::handleAt(const QPoint &pos)const
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
            // _mode = Mode::Resizing;
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

void SnippingOverlay::moveSelectionBy(const QPointF &d)
{
    QRect r = _sel.translated(d.toPoint());
    if (r.left() < rect().left())       r.moveLeft(rect().left());
    if (r.top() < rect().top())         r.moveTop(rect().top());
    if (r.right() > rect().right())     r.moveRight(rect().right());
    if (r.bottom() > rect().bottom())   r.moveBottom(rect().bottom());
    const QPoint applied = r.topLeft() - _sel.topLeft();
    if (applied.isNull())
        return;

    for(Annotation &a:_anns){
        a.textPos+=d;
        a.p1 +=d;
        a.p2 +=d;
        a.rect = a.rect.translated(d.toPoint());
        a.path =a.path.translated(d.toPoint());
    }

    _sel = r;
}

void SnippingOverlay::updateDraft(const QPoint &pos)
{
    switch (_tool){
    case Tool::Mosaic:
    case Tool::Rect:{
        _draft.rect = QRectF(_pressPos,pos).normalized();
        break;
    }
    case Tool::Arrow:{
        _draft.p2 = pos;
        break;
    }
    case Tool::Pen:{
        if (!_penPoints.isEmpty() && QLineF(_penPoints.last(),QPointF(pos)).length() < 2.0){
            // 点太密
            return;
        }
        _penPoints << pos;
        _draft.path = smoothPath(_penPoints);
    }
    default:
        break;
    }
}

void SnippingOverlay::pushUndo()
{
    _undo.append(_anns);
    if (_undo.size() >40){
        _undo.removeFirst();
    }
    _redo.clear();
}

void SnippingOverlay::popUndo()
{
    if (!_undo.isEmpty())
        _undo.removeLast();
    _bar->setUndoEnabled(false);
}

void SnippingOverlay::undo()
{
    if (_undo.isEmpty()){
        _bar->setUndoEnabled(false);
        return;
    }
    _redo.append(_anns);
    _anns = _undo.takeLast();
    update();
}

void SnippingOverlay::redo()
{
    if (_redo.isEmpty()){
        return;
    }
    _undo.append(_anns);
    _anns = _redo.takeLast();
    update();
}

void SnippingOverlay::clearAnnotations()
{
    if (_anns.isEmpty())return;
    pushUndo();
    _anns.clear();
    update();
}

void SnippingOverlay::beginTextInput(const QPoint &pos)
{
    if (_textEdit){
        commitText();
    }

    _textAnchor = pos;

    QFont f = Theme::uiFont(1,true);
    f.setPixelSize(_fontSize);

    _textEdit  = new QLineEdit(this);
    _textEdit->setFrame(false);
    _textEdit->setFont(f);
    _textEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: rgba(0, 0, 0, 180);"
        "  color: %1;"
        "  border: 1px dashed %2;"
        "  padding: 2px 4px;"
        "  selection-background-color: %2;"
                                 "}").arg(_color.name(), Theme::kSelBorder.name()));

    QFontMetrics fm(f);
    _textEdit->move(pos);
    _textEdit->resize(240,fm.height()+12);
    _textEdit->show();
    _textEdit->setFocus();

    connect(_textEdit, &QLineEdit::returnPressed,
            this, &SnippingOverlay::commitText);
    update();

}

void SnippingOverlay::commitText()
{
    if (!_textEdit)
        return;

    const QString&text = _textEdit->text().trimmed();
    _textEdit ->hide();
    _textEdit->deleteLater();
    _textEdit = nullptr;

    if (!text.isEmpty()){
        pushUndo();
        Annotation a;
        a.tool = Tool::Text;
        a.color = _color;
        a.text =text;
        a.fontSize = _fontSize;
        a.textPos = _textAnchor;
        _anns.append(a);
    }
    setFocus();
    update();
}
