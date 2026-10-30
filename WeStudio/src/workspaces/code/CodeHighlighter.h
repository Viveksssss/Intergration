#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

// ---------------------------------------------------------------------------
//  C++ 语法高亮。
//
//  核心只有一件事：Qt 会在某个 block（= 一行）内容变化时，
//  只调用 highlightBlock() 重刷这一行 —— 这就是「增量」的来源。
//  但跨行结构（/* ... */）必须靠 previousBlockState / setCurrentBlockState
//  在行与行之间传递状态，否则编辑器永远不知道上一行是不是还在注释里。
// ---------------------------------------------------------------------------
class CodeHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit CodeHighlighter(QTextDocument *parent = nullptr);

    // block 状态值。0 是默认状态；正数是我们自己定义的「跨行状态」。
    enum State {
        Normal         = 0,
        InBlockComment = 1,
    };

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };

    void buildRules();
    void applyBlockComment(const QString &text);

    QVector<Rule>   m_rules;
    QTextCharFormat m_commentFmt;
    QTextCharFormat m_stringFmt;
};
