#include "SnippingOverlay.h"

#include "ShotCapture.h"
#include "ShotTheme.h"
#include "ToolBar.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStandardPaths>

namespace {

constexpr int kMinSel    = 8;    // 选区最小边长
constexpr int kHandleHit = 4;    // 手柄的额外命中范围（手感好一点）

// ---------------------------------------------------------------------------
//  画笔平滑：中点法二次贝塞尔
//
//  直接把采样点用 lineTo 连起来，鼠标走得快时会看到一段段折线。
//  改成「以采样点为控制点、以相邻两点中点为终点」的二次曲线，
//  因为曲线在终点处的切线方向正好是控制点连线的方向，
//  相邻两段在公共中点处切线相同 → 一阶连续 → 肉眼看不出拐角。
// ---------------------------------------------------------------------------
QPainterPath smoothPath(const QVector<QPointF> &pts)
{
    QPainterPath path;
    if (pts.isEmpty())
        return path;

    if (pts.size() == 1) {
        // 点一下也算一笔：画一个极短线段，靠 RoundCap 显示成圆点
        path.moveTo(pts[0]);
        path.lineTo(pts[0] + QPointF(0.1, 0.1));
        return path;
    }

    path.moveTo(pts[0]);
    if (pts.size() == 2) {
        path.lineTo(pts[1]);
        return path;
    }

    for (int i = 1; i < pts.size() - 1; ++i) {
        const QPointF mid = (pts[i] + pts[i + 1]) * 0.5;
        path.quadTo(pts[i], mid);
    }
    path.lineTo(pts.last());
    return path;
}

}  // namespace

// ===========================================================================
//  构造
// ===========================================================================
SnippingOverlay::SnippingOverlay(const QImage &shot, QWidget *parent)
    : QWidget(parent)
    , m_shot(shot)
{
    // Frameless + 置顶 是截图工具的标准配方：
    // 没有标题栏 → 能盖住整个屏幕；置顶 → 盖住任务栏和其它窗口
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMouseTracking(true);              // 不按下鼠标也要收到 move，十字线要用
    setFocusPolicy(Qt::StrongFocus);     // 不然收不到 Esc / Enter
    setCursor(Qt::CrossCursor);

    setGeometry(ShotCapture::virtualGeometry());
    m_mousePos = rect().center();
    m_color = ShotTheme::kInkRed;

    m_bar = new ToolBar(this);
    m_bar->hide();

    connect(m_bar, &ToolBar::toolChanged, this, [this](Tool t) {
        selectTool(t);
        setFocus();                      // 点完按钮把键盘焦点抢回来
    });
    connect(m_bar, &ToolBar::undoRequested, this, [this] {
        undo();
        setFocus();
    });
    connect(m_bar, &ToolBar::clearRequested, this, [this] {
        clearAnnotations();
        setFocus();
    });
    connect(m_bar, &ToolBar::copyRequested, this, &SnippingOverlay::finishAndCopy);
    connect(m_bar, &ToolBar::saveRequested, this, &SnippingOverlay::saveToFile);
    connect(m_bar, &ToolBar::cancelRequested, this, [this] {
        emit cancelled();
        close();
    });
}

SnippingOverlay::~SnippingOverlay() = default;

void SnippingOverlay::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    setGeometry(ShotCapture::virtualGeometry());
    layoutToolBar();
    setFocus();
}

void SnippingOverlay::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    layoutToolBar();
}

// ===========================================================================
//  绘制
// ===========================================================================
void SnippingOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // ---- 1. 先把"定格的屏幕"整张贴上来
    if (!m_shot.isNull())
        p.drawImage(QPoint(0, 0), m_shot);
    else
        p.fillRect(rect(), Qt::black);

    // ---- 2. 遮罩：even-odd 填充规则下，被包含的内圈会被挖空
    //         这就是"外面半透明、选区里清晰"的全部秘密
    QPainterPath mask;
    mask.setFillRule(Qt::OddEvenFill);
    mask.addRect(QRectF(rect()));
    if (m_hasSel)
        mask.addRect(QRectF(m_sel));
    p.fillPath(mask, ShotTheme::kMask);

    // ---- 3. 标注。裁剪到选区，超出的部分不会盖上遮罩
    if (m_hasSel) {
        p.save();
        p.setClipRect(m_sel);
        for (const Annotation &a : m_anns)
            ShotCapture::paintOne(p, m_shot, a);
        if (m_hasDraft)
            ShotCapture::paintOne(p, m_shot, m_draft);
        p.restore();
    }

    // ---- 4. 选区装饰（边框、8 个手柄、尺寸标签）
    if (m_hasSel)
        paintSelectionChrome(p);

    // ---- 5. 十字辅助线（正在落笔时太干扰，藏起来）
    if (m_mode != Mode::Drawing && !m_textEdit)
        paintCrosshair(p);
}

void SnippingOverlay::paintSelectionChrome(QPainter &p)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setBrush(Qt::NoBrush);

    // 白线打底 + 蓝线压里：深色浅色背景上都看得见
    p.setPen(QPen(QColor(255, 255, 255, 210), 1));
    p.drawRect(m_sel.adjusted(0, 0, -1, -1));
    p.setPen(QPen(ShotTheme::kSelBorder, 1));
    p.drawRect(m_sel.adjusted(1, 1, -2, -2));

    // 8 个手柄：正在画的时候藏起来，免得挡视线
    if (m_mode != Mode::Drawing) {
        static const Handle kHandles[] = {
            Handle::TopLeft, Handle::Top,    Handle::TopRight, Handle::Right,
            Handle::BottomRight, Handle::Bottom, Handle::BottomLeft, Handle::Left
        };
        p.setPen(QPen(ShotTheme::kSelBorder, 1));
        p.setBrush(ShotTheme::kHandleFill);
        for (Handle h : kHandles)
            p.drawRect(handleRect(h));
    }

    // 尺寸标签：贴着选区左上角，顶到屏幕上边就挪到里面
    const QString txt = QStringLiteral("%1 × %2")
                            .arg(m_sel.width())
                            .arg(m_sel.height());
    QFont f = ShotTheme::uiFont(8, true);
    f.setPixelSize(12);
    p.setFont(f);

    const QFontMetrics fm(f);
    const int tw = fm.horizontalAdvance(txt) + 12;
    const int th = fm.height() + 6;

    QRect tag(m_sel.left(), m_sel.top() - th - 6, tw, th);
    if (tag.top() < 0)
        tag.moveTop(m_sel.top() + 6);

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(ShotTheme::kSizeTagBg);
    p.drawRoundedRect(tag, 3, 3);
    p.setPen(Qt::white);
    p.drawText(tag, Qt::AlignCenter, txt);

    p.restore();
}

void SnippingOverlay::paintCrosshair(QPainter &p)
{
    if (!rect().contains(m_mousePos))
        return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(ShotTheme::kCross, 1, Qt::DashLine));
    p.drawLine(rect().left(), m_mousePos.y(), rect().right(), m_mousePos.y());
    p.drawLine(m_mousePos.x(), rect().top(), m_mousePos.x(), rect().bottom());
    p.restore();
}

// ===========================================================================
//  手柄
// ===========================================================================
QRect SnippingOverlay::handleRect(Handle h) const
{
    const int s = 7;
    QPoint c;
    switch (h) {
    case Handle::TopLeft:     c = QPoint(m_sel.left(), m_sel.top());           break;
    case Handle::Top:         c = QPoint(m_sel.center().x(), m_sel.top());     break;
    case Handle::TopRight:    c = QPoint(m_sel.right(), m_sel.top());          break;
    case Handle::Right:       c = QPoint(m_sel.right(), m_sel.center().y());   break;
    case Handle::BottomRight: c = QPoint(m_sel.right(), m_sel.bottom());       break;
    case Handle::Bottom:      c = QPoint(m_sel.center().x(), m_sel.bottom());  break;
    case Handle::BottomLeft:  c = QPoint(m_sel.left(), m_sel.bottom());        break;
    case Handle::Left:        c = QPoint(m_sel.left(), m_sel.center().y());    break;
    case Handle::None:
    case Handle::Move:
        return QRect();
    }
    return QRect(c.x() - s / 2, c.y() - s / 2, s, s);
}

Handle SnippingOverlay::handleAt(const QPoint &pos) const
{
    if (!m_hasSel)
        return Handle::None;

    static const Handle kHandles[] = {
        Handle::TopLeft, Handle::Top,    Handle::TopRight, Handle::Right,
        Handle::BottomRight, Handle::Bottom, Handle::BottomLeft, Handle::Left
    };
    for (Handle h : kHandles) {
        if (handleRect(h).adjusted(-kHandleHit, -kHandleHit,
                                   kHandleHit, kHandleHit).contains(pos))
            return h;
    }

    if (m_sel.contains(pos))
        return Handle::Move;

    return Handle::None;
}

void SnippingOverlay::applyCursorShape(const QPoint &pos)
{
    if (m_tool != Tool::None && m_hasSel && m_sel.contains(pos)) {
        setCursor(Qt::CrossCursor);
        return;
    }

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

// ===========================================================================
//  鼠标
// ===========================================================================
void SnippingOverlay::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;

    const QPoint pos = e->pos();
    m_mousePos = pos;
    m_lastPos  = pos;

    // 有任何未提交的文字，先落定
    if (m_textEdit)
        commitText();

    // ---- A. 工具激活 + 点在选区内 → 开始画
    if (m_tool != Tool::None && m_hasSel && m_sel.contains(pos)) {
        if (m_tool == Tool::Text) {
            beginTextInput(pos);
            return;
        }

        // 落笔起点。注意这个成员在"框选"分支里也用来记起点，
        // 忘了在这里刷新，矩形/马赛克就会从上一次框选的位置开始画。
        m_pressPos = pos;

        pushUndo();
        m_draft = Annotation();
        m_draft.tool   = m_tool;
        m_draft.color  = m_color;
        m_draft.width  = m_penWidth;
        m_draft.block  = m_block;
        m_draft.p1     = pos;
        m_draft.p2     = pos;
        m_draft.rect   = QRectF(pos, QSizeF(0, 0));

        if (m_tool == Tool::Pen) {
            m_penPoints.clear();
            m_penPoints << pos;
        }

        m_hasDraft = true;
        m_mode = Mode::Drawing;
        update();
        return;
    }

    // ---- B. 按在手柄 / 选区内部 → 调整选区
    const Handle h = handleAt(pos);
    if (h != Handle::None) {
        m_handle = h;
        m_mode = (h == Handle::Move) ? Mode::Moving : Mode::Resizing;
        m_pressSel = m_sel;
        return;
    }

    // ---- C. 其它地方 → 重新框一个选区，旧标注作废
    m_hasSel   = false;
    m_anns.clear();
    m_undo.clear();
    m_redo.clear();
    m_hasDraft = false;
    m_mode     = Mode::Framing;
    m_pressPos = pos;
    m_sel      = QRect(pos, QSize(0, 0));
    if (m_bar) {
        m_bar->hide();
        m_bar->setUndoEnabled(false);
    }
    update();
}

void SnippingOverlay::mouseMoveEvent(QMouseEvent *e)
{
    const QPoint pos = e->pos();
    m_mousePos = pos;

    switch (m_mode) {
    case Mode::Idle:
        applyCursorShape(pos);
        update();                        // 十字线跟着走
        return;

    case Mode::Framing:
        m_sel = QRect(m_pressPos, pos).normalized().intersected(rect());
        m_hasSel = m_sel.width() >= 2 && m_sel.height() >= 2;
        update();
        return;

    case Mode::Moving: {
        const QPoint d = pos - m_lastPos;
        if (!d.isNull())
            moveSelectionBy(d);
        m_lastPos = pos;
        update();
        return;
    }

    case Mode::Resizing: {
        QRect r = m_pressSel;
        switch (m_handle) {
        case Handle::Left:
        case Handle::TopLeft:
        case Handle::BottomLeft:  r.setLeft(pos.x());   break;
        case Handle::Right:
        case Handle::TopRight:
        case Handle::BottomRight: r.setRight(pos.x());  break;
        default: break;
        }
        switch (m_handle) {
        case Handle::Top:
        case Handle::TopLeft:
        case Handle::TopRight:    r.setTop(pos.y());    break;
        case Handle::Bottom:
        case Handle::BottomLeft:
        case Handle::BottomRight: r.setBottom(pos.y()); break;
        default: break;
        }

        r = r.normalized().intersected(rect());
        // 缩到比手柄还小就没法再抓了，直接不更新
        if (r.width() >= kMinSel && r.height() >= kMinSel) {
            m_sel = r;
            layoutToolBar();
        }
        update();
        return;
    }

    case Mode::Drawing:
        updateDraft(pos);
        update();
        return;
    }
}

void SnippingOverlay::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;

    if (m_mode == Mode::Drawing) {
        if (m_tool == Tool::Pen)
            m_draft.path = smoothPath(m_penPoints);

        if (m_draft.isEmpty()) {
            popUndo();       // 这一笔什么都没画出来，撤销点也不该留
        } else {
            m_anns.append(m_draft);
            if (m_bar)
                m_bar->setUndoEnabled(true);
        }

        m_hasDraft = false;
        m_penPoints.clear();
        m_mode = Mode::Idle;
        applyCursorShape(e->pos());
        update();
        return;
    }

    if (m_mode == Mode::Framing) {
        m_mode = Mode::Idle;
        layoutToolBar();          // 框完了才把工具栏摆出来
        applyCursorShape(e->pos());
        update();
        return;
    }

    m_mode = Mode::Idle;
    m_handle = Handle::None;
    applyCursorShape(e->pos());
    update();
}

void SnippingOverlay::mouseDoubleClickEvent(QMouseEvent *e)
{
    // 只在"调整选区"模式下双击才等于完成，避免画图时误触
    if (m_tool == Tool::None && m_hasSel && m_sel.contains(e->pos()))
        finishAndCopy();
}

// ===========================================================================
//  选区移动：标注要跟着走
// ===========================================================================
void SnippingOverlay::moveSelectionBy(const QPoint &d)
{
    QRect r = m_sel.translated(d);

    // 不许拖出屏幕
    if (r.left() < rect().left())       r.moveLeft(rect().left());
    if (r.top() < rect().top())         r.moveTop(rect().top());
    if (r.right() > rect().right())     r.moveRight(rect().right());
    if (r.bottom() > rect().bottom())   r.moveBottom(rect().bottom());

    // 用"实际挪了多少"而不是"想挪多少"，否则贴边时标注会慢慢跑偏
    const QPoint applied = r.topLeft() - m_sel.topLeft();
    if (applied.isNull())
        return;

    m_sel = r;
    for (Annotation &a : m_anns)
        a.translateBy(applied);
    if (m_hasDraft)
        m_draft.translateBy(applied);

    layoutToolBar();
}

// ===========================================================================
//  标注草稿
// ===========================================================================
void SnippingOverlay::updateDraft(const QPoint &pos)
{
    switch (m_tool) {
    case Tool::Rect:
    case Tool::Mosaic:
        m_draft.rect = QRectF(m_pressPos, pos).normalized();
        break;

    case Tool::Arrow:
        m_draft.p2 = pos;
        break;

    case Tool::Pen: {
        // 太密的点没有信息量，只会让路径变长、渲染变慢
        if (!m_penPoints.isEmpty()
            && QLineF(m_penPoints.last(), QPointF(pos)).length() < 2.0)
            return;
        m_penPoints << pos;
        m_draft.path = smoothPath(m_penPoints);
        break;
    }

    default:
        break;
    }
}

// ===========================================================================
//  撤销栈：直接存整份标注数组
//
//  标注数量一般几十个、每个几十字节，拷贝一份远比"记录操作类型再反算"
//  简单可靠。真到了上千标注再换增量方案。
// ===========================================================================
void SnippingOverlay::pushUndo()
{
    m_undo.append(m_anns);
    if (m_undo.size() > 40)
        m_undo.removeFirst();
    m_redo.clear();
    if (m_bar)
        m_bar->setUndoEnabled(true);
}

void SnippingOverlay::popUndo()
{
    if (!m_undo.isEmpty())
        m_undo.removeLast();
    if (m_bar)
        m_bar->setUndoEnabled(!m_undo.isEmpty());
}

void SnippingOverlay::undo()
{
    if (m_undo.isEmpty())
        return;
    m_redo.append(m_anns);
    m_anns = m_undo.takeLast();
    if (m_bar)
        m_bar->setUndoEnabled(!m_undo.isEmpty());
    update();
}

void SnippingOverlay::redo()
{
    if (m_redo.isEmpty())
        return;
    m_undo.append(m_anns);
    m_anns = m_redo.takeLast();
    if (m_bar)
        m_bar->setUndoEnabled(true);
    update();
}

void SnippingOverlay::clearAnnotations()
{
    if (m_anns.isEmpty())
        return;
    pushUndo();
    m_anns.clear();
    update();
}

void SnippingOverlay::selectTool(Tool t)
{
    m_tool = t;
    if (m_bar)
        m_bar->setCurrentTool(t);
    applyCursorShape(m_mousePos);
    update();
}

// ===========================================================================
//  文字：先放一个 QLineEdit 在原地，回车才变成标注
// ===========================================================================
void SnippingOverlay::beginTextInput(const QPoint &pos)
{
    if (m_textEdit)
        commitText();

    m_textAnchor = pos;

    QFont f = ShotTheme::uiFont(1, true);
    f.setPixelSize(m_fontSize);

    m_textEdit = new QLineEdit(this);
    m_textEdit->setFrame(false);
    m_textEdit->setFont(f);
    m_textEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: rgba(0, 0, 0, 180);"
        "  color: %1;"
        "  border: 1px dashed %2;"
        "  padding: 2px 4px;"
        "  selection-background-color: %2;"
        "}").arg(m_color.name(), ShotTheme::kSelBorder.name()));

    const QFontMetrics fm(f);
    m_textEdit->move(pos);
    m_textEdit->resize(240, fm.height() + 12);
    m_textEdit->show();
    m_textEdit->setFocus();

    connect(m_textEdit, &QLineEdit::returnPressed,
            this, &SnippingOverlay::commitText);

    update();
}

void SnippingOverlay::commitText()
{
    if (!m_textEdit)
        return;

    const QString txt = m_textEdit->text().trimmed();

    // 必须 deleteLater：现在可能正处在 QLineEdit 自己的信号里
    m_textEdit->hide();
    m_textEdit->deleteLater();
    m_textEdit = nullptr;

    if (!txt.isEmpty()) {
        pushUndo();
        Annotation a;
        a.tool     = Tool::Text;
        a.color    = m_color;
        a.text     = txt;
        a.fontSize = m_fontSize;
        a.textPos  = m_textAnchor;
        m_anns.append(a);
    }

    setFocus();
    update();
}

// ===========================================================================
//  工具栏定位
// ===========================================================================
void SnippingOverlay::layoutToolBar()
{
    if (!m_bar)
        return;
    if (!m_hasSel) {
        m_bar->hide();
        return;
    }
    m_bar->placeNear(m_sel, rect());
    m_bar->show();
    m_bar->raise();
}

// ===========================================================================
//  收尾
// ===========================================================================
QImage SnippingOverlay::result() const
{
    if (!m_hasSel)
        return QImage();
    return ShotCapture::renderRegion(m_shot, m_sel, m_anns);
}

void SnippingOverlay::finishAndCopy()
{
    const QImage img = result();
    if (img.isNull())
        return;
    emit finished(img);
    close();
}

void SnippingOverlay::saveToFile()
{
    const QImage img = result();
    if (img.isNull())
        return;

    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();

    const QString suggested = QStringLiteral("%1/Snip_%2.png")
        .arg(dir, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存截图"), suggested,
        QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg)"));
    if (path.isEmpty())
        return;

    if (!img.save(path)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入：\n%1").arg(path));
        return;
    }

    emit finished(img);
    close();
}

// ===========================================================================
//  键盘
// ===========================================================================
void SnippingOverlay::keyPressEvent(QKeyEvent *e)
{
    // 文字输入框有焦点时，除了 Esc 都不拦
    if (m_textEdit) {
        if (e->key() == Qt::Key_Escape) {
            m_textEdit->hide();
            m_textEdit->deleteLater();
            m_textEdit = nullptr;
            setFocus();
            update();
            return;
        }
        QWidget::keyPressEvent(e);
        return;
    }

    const bool ctrl  = e->modifiers() & Qt::ControlModifier;
    const bool shift = e->modifiers() & Qt::ShiftModifier;

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
        if (!m_hasSel)
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

    // 字母切工具（避开带修饰键的组合，免得和 Ctrl+C 之类打架）
    if (!ctrl && !(e->modifiers() & Qt::AltModifier)) {
        switch (e->key()) {
        case Qt::Key_R: selectTool(Tool::Rect);   return;
        case Qt::Key_A: selectTool(Tool::Arrow);  return;
        case Qt::Key_P: selectTool(Tool::Pen);    return;
        case Qt::Key_T: selectTool(Tool::Text);   return;
        case Qt::Key_M: selectTool(Tool::Mosaic); return;
        default: break;
        }
    }

    QWidget::keyPressEvent(e);
}

// ===========================================================================
//  自检入口
// ===========================================================================
void SnippingOverlay::setDemoScene(const QRect &sel, const QVector<Annotation> &anns)
{
    m_sel    = sel;
    m_hasSel = sel.isValid() && sel.width() > 2 && sel.height() > 2;
    m_anns   = anns;
    if (m_hasSel)
        m_sel = m_sel.intersected(rect());
    layoutToolBar();
    update();
}

// ===========================================================================
//  自检：把关键状态压成一行
// ===========================================================================
QString SnippingOverlay::debugSnapshot() const
{
    QString s = QStringLiteral("sel=%1,%2,%3,%4 anns=%5")
                    .arg(m_sel.x()).arg(m_sel.y())
                    .arg(m_sel.width()).arg(m_sel.height())
                    .arg(m_anns.size());

    for (int i = 0; i < m_anns.size(); ++i) {
        const Annotation &a = m_anns.at(i);
        s += QStringLiteral(" |#%1 t=%2 r=%3,%4,%5,%6")
                 .arg(i)
                 .arg(int(a.tool))
                 .arg(qRound(a.rect.x())).arg(qRound(a.rect.y()))
                 .arg(qRound(a.rect.width())).arg(qRound(a.rect.height()));
    }
    return s;
}
