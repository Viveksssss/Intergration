#include "LineNumberArea.h"
#include "CodeEditor.h"

LineNumberArea::LineNumberArea(CodeEditor *editor)
    : QWidget(editor)
    , m_editor(editor)
{
    // 鼠标跟踪打开，才能收到「没按键时的移动」事件 —— 折叠三角要用它做悬停高亮
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    // 真正画行号的是 CodeEditor —— 只有它拿得到 firstVisibleBlock() /
    // blockBoundingGeometry() / contentOffset()，这三个都是 protected。
    // 所以这里转一手，把样式和计算都留在编辑器里。
    m_editor->lineNumberAreaPaintEvent(event);
}

void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_editor->handleGutterClick(event->pos());
    QWidget::mousePressEvent(event);
}

void LineNumberArea::mouseMoveEvent(QMouseEvent *event)
{
    m_editor->handleGutterHover(event->pos());
    QWidget::mouseMoveEvent(event);
}

void LineNumberArea::leaveEvent(QEvent *event)
{
    m_editor->handleGutterHover(QPoint(-1, -1));
    QWidget::leaveEvent(event);
}
