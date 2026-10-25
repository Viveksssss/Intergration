#include "linenumberarea.h"
#include "codeeditor.h"

LineNumberArea::LineNumberArea(CodeEditor *parent)
    : QWidget(parent)
    , _editor(parent)
{
    setCursor(Qt::ArrowCursor);
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(_editor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    _editor->lineNumberAreaPaintEvent(event);
}
