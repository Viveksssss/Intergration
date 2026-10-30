#pragma once

#include <QWidget>

class CodeEditor;

// ---------------------------------------------------------------------------
//  行号栏。
//
//  它就是一个普通的 QWidget，靠 QPlainTextEdit::setViewportMargins()
//  在正文左边「挤」出一块地方，然后自己画。
//
//  为什么不用 QPlainTextEdit 自带的 paintEvent 画？
//  因为正文会被滚动裁剪，行号栏要跟着滚但不是同一套坐标。
//  拆成独立 widget 后，滚动同步只需要一句 lineNumberArea->scroll(0, dy)。
// ---------------------------------------------------------------------------
class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    explicit LineNumberArea(CodeEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    CodeEditor *m_editor;
};
