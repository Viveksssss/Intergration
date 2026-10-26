#include "linenumberarea.h"
#include "codeeditor.h"

LineNumberArea::LineNumberArea(CodeEditor *parent)
    : QWidget(parent)
    , _editor(parent)
{
    setCursor(Qt::ArrowCursor);
    setMouseTracking(true);
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    _editor->lineNumberAreaPaintEvent(event);
}

void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button()  == Qt::LeftButton){
        _editor->handleGutterClick(event->pos());
    }
    QWidget::mousePressEvent(event);
}

void LineNumberArea::mouseMoveEvent(QMouseEvent *event)
{
    _editor->handleGutterHover(event->pos());
    QWidget::mouseMoveEvent(event);
}

void LineNumberArea::leaveEvent(QEvent *event)
{
    _editor->handleGutterHover(QPoint(-1, -1));
    QWidget::leaveEvent(event);
}
