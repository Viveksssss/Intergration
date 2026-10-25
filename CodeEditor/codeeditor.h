#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>

class CodeHighlighter;
class LineNumberArea;
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QWidget*parent = nullptr);

    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    void gotoline(int lineNumber);
    void setWordWrapEnabled(bool on = false);
    QString statsText()const;

signals:
    void cursorMoved(int line,int column);

private slots:
    void onCursorPositionChanged();
    void onUpdateRequest(const QRect&rect,int dy);
    void onBlockCountChanged(int newBlockCount);
private:
    void updateGutterWidth();
    void refreshExtraSelections();
    // QWidget interface
protected:
    virtual void resizeEvent(QResizeEvent *event) override;


private:
    LineNumberArea*_gutter = nullptr;
    CodeHighlighter*_highlighter = nullptr;
};


#endif // CODEEDITOR_H
