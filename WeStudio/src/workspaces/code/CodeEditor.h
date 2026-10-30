#pragma once

#include <QPlainTextEdit>

#include "BracketMatcher.h"
#include "FoldManager.h"

class LineNumberArea;
class CodeHighlighter;

// ---------------------------------------------------------------------------
//  代码编辑器本体。
//
//  QPlainTextEdit 是「一个 viewport + 一堆 block」的模型：
//    · 每个 block 就是一行，QTextBlock 能直接当迭代器用
//    · 它按需只为可见的 block 做布局（QPlainTextDocumentLayout），
//      所以十万行也不怕 —— 这是它和 QTextEdit 最大的区别
//
//  本类在它上面加四件事：
//    ① 行号栏（靠 viewportMargins 挤出一块空地）
//    ② 当前行/括号匹配高亮（ExtraSelection）
//    ③ 折叠（FoldManager 改 QTextBlock::setVisible）
//    ④ 折叠提示条（重写 paintEvent，在文字层之上再画一层）
// ---------------------------------------------------------------------------
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    // ── 行号栏相关的 public 接口（LineNumberArea 会回调过来）
    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    void handleGutterClick(const QPoint &pos);
    void handleGutterHover(const QPoint &pos);

    // ── 折叠
    void toggleFoldAtCursor();
    void foldAll();
    void unfoldAll();
    bool foldsAreCollapsed(int blockNumber) const;

    // ── 其它
    void gotoLine(int lineNumber);            // 1-based
    void setWordWrapEnabled(bool on);
    QString statsText() const;

signals:
    void cursorMoved(int line, int column);   // 1-based
    void foldStateChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onBlockCountChanged(int newBlockCount);
    void onUpdateRequest(const QRect &rect, int dy);
    void onCursorPositionChanged();
    void onTextChanged();
    void onContentsChange(int position, int charsRemoved, int charsAdded);

private:
    void updateGutterWidth();
    void refreshExtraSelections();
    void revealCursor();
    void rehighlightVisible();
    void drawFoldHints(QPainter &painter);

    int   blockNumberAtViewportY(int y) const;
    QRect blockRectInViewport(int blockNumber) const;

    LineNumberArea  *m_gutter      = nullptr;
    CodeHighlighter *m_highlighter = nullptr;
    FoldManager      m_folds;

    BracketMatcher::Result m_bracketPair;
    int  m_hoverBlock  = -1;
    bool m_rescanning  = false;

    // 增量判断用：上一次改动删了多少、加了什么、行数多少
    int     m_lastRemoved  = 0;
    QString m_lastAddedText;
    int     m_lastBlockCount = -1;
};
