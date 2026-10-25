#ifndef LINENUMBERAREA_H
#define LINENUMBERAREA_H

#include <QWidget>

class CodeEditor;
class LineNumberArea : public QWidget
{
    Q_OBJECT
public:
    explicit LineNumberArea(CodeEditor *parent = nullptr);

    QSize sizeHint()const override;

protected:
    void paintEvent(QPaintEvent*event)override;

signals:

private:
    CodeEditor*_editor;
};

#endif // LINENUMBERAREA_H
