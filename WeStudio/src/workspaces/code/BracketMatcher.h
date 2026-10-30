#pragma once

#include <QString>
#include <QTextBlock>

class QTextDocument;

// ---------------------------------------------------------------------------
//  括号匹配：给定光标位置，判断光标左右是否有括号，
//  以及它的配对括号在文档的哪个字符位置。
//
//  这是纯逻辑，不依赖任何 widget，所以可以单独写测试。
//  CodeEditor 只负责把结果画成 ExtraSelection。
// ---------------------------------------------------------------------------
class BracketMatcher
{
public:
    struct Result
    {
        bool    valid   = false;
        int     nearPos = -1;    // 光标旁边那个括号的绝对位置
        int     matePos = -1;    // 配对括号的绝对位置
        bool    mastered = false; // 配对括号是否在当前视口内（暂未使用，保留）
    };

    // cursorPos 是 QTextCursor::position()，即「字符间隙」编号。
    // 一个字符的间隙 i 表示它左边是第 i-1 个字符、右边是第 i 个字符。
    static Result match(QTextDocument *doc, int cursorPos);

    // 仅供测试/调试：'(' -> ')'，'{' -> '}'，'[' -> ']'
    static QChar mateOf(QChar ch);
    static bool  isOpen(QChar ch);
    static bool  isClose(QChar ch);
};
