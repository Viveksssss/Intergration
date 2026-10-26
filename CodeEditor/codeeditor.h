#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include "foldmanager.h"
#include "BracketMatcher.h"
class CodeHighlighter;
class LineNumberArea;
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QWidget*parent = nullptr);

    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    void handleGutterClick(const QPoint &pos);
    void handleGutterHover(const QPoint &pos);

    // ── 折叠
    void toggleFoldAtCursor();
    void foldAll();
    void unfoldAll();
    bool foldsAreCollapsed(int blockNumber) const;

    void gotoline(int lineNumber);
    void setWordWrapEnabled(bool on = false);
    QString statsText()const;

signals:
    void cursorMoved(int line,int column);
    void foldStateChanged();

private slots:
    void onCursorPositionChanged();
    void onUpdateRequest(const QRect&rect,int dy);
    void onBlockCountChanged(int newBlockCount);
    void onTextChanged();
private:
    void updateGutterWidth();
    void refreshExtraSelections();
    void revealCursor();
    void drawFoldHints(QPainter&painter);
    void onContentsChange(int position, int charsRemoved, int charsAdded);

    int   blockNumberAtViewportY(int y) const;
    QRect blockRectInViewport(int blockNumber) const;
    // QWidget interface
protected:
    virtual void resizeEvent(QResizeEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;


private:
    LineNumberArea*_gutter = nullptr;
    CodeHighlighter*_highlighter = nullptr;
    FoldManager _folds;

    BracketMatcher::Result _brackPair;
    int _hoverBlock = -1;
    bool _rescanning = false;

    int     _lastRemoved  = 0;
    QString _lastAddedText;
    int     _lastBlockCount = -1;
};




#endif // CODEEDITOR_H
