#include "BracketMatcher.h"

#include <QTextCursor>
#include <QTextDocument>

namespace {

const QString kOpen  = QStringLiteral("([{");
const QString kClose = QStringLiteral(")]}");
const QString kAll   = QStringLiteral("()[]{}");

// 向后（direction = +1）或向前（direction = -1）扫描，找出配对括号。
// 起点是 (block, offsetInBlock)，返回绝对字符位置；找不到返回 -1。
int scanForMate(QTextDocument *doc, QTextBlock block, int offsetInBlock,
                QChar open, QChar close, int direction, int limit)
{
    // 关键：起点那个括号已经被调用方跳过了（正向传 offset+1，反向传 offset-1），
    // 相当于「已经读入了一层」，所以 depth 从 1 开始，而不是 0。
    //
    // 写成 0 的后果：正向扫描遇到的第一个 '}' 会把 depth 减到 -1，
    // 之后怎么扫都回不到 0，永远找不到配对 —— 括号匹配整个失效。
    int depth = 1;
    int steps = 0;

    while (block.isValid()) {
        const QString text = block.text();

        // 正向从 offsetInBlock 到末尾；反向从 offsetInBlock-1 到 0
        int i = (direction > 0) ? offsetInBlock : qMin(offsetInBlock, text.size()) - 1;
        while (i >= 0 && i < text.size()) {
            const QChar ch = text.at(i);
            if (ch == open) {
                ++depth;
            } else if (ch == close) {
                --depth;
                if (depth == 0)
                    return block.position() + i;
            }
            if (++steps > limit)
                return -1;
            i += direction;
        }

        block = (direction > 0) ? block.next() : block.previous();
        if (block.isValid())
            offsetInBlock = (direction > 0) ? 0 : block.text().size();
    }
    return -1;
}

}  // namespace

bool BracketMatcher::isOpen(QChar ch)  { return kOpen.contains(ch); }
bool BracketMatcher::isClose(QChar ch) { return kClose.contains(ch); }

QChar BracketMatcher::mateOf(QChar ch)
{
    const int idx = kAll.indexOf(ch);
    if (idx < 0) return QChar();
    // kAll 的排列是 "()[]{}"，同一对的下标是 i^1（0<->1, 2<->3, 4<->5）
    return kAll.at(idx ^ 1);
}

BracketMatcher::Result BracketMatcher::match(QTextDocument *doc, int cursorPos)
{
    Result r;
    if (!doc || cursorPos < 0)
        return r;

    const int docLen = doc->characterCount() - 1;   // 最后一个位置是文档末尾
    if (cursorPos > docLen)
        return r;

    // 候选：光标左边一个字符、右边一个字符。
    // 先查左边，再查右边 —— 这和 IDE 的直觉一致：
    // 光标停在 "}" 后面时，高亮的是 "}" 而不是它右边的字符。
    struct Cand { int pos; QChar ch; };
    Cand cands[2];
    int n = 0;

    if (cursorPos - 1 >= 0 && cursorPos - 1 < docLen) {
        QTextCursor c(doc);
        c.setPosition(cursorPos - 1);
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        const QString s = c.selectedText();
        if (!s.isEmpty())
            cands[n++] = { cursorPos - 1, s.at(0) };
    }
    if (cursorPos < docLen) {
        QTextCursor c(doc);
        c.setPosition(cursorPos);
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        const QString s = c.selectedText();
        if (!s.isEmpty())
            cands[n++] = { cursorPos, s.at(0) };
    }

    for (int i = 0; i < n; ++i) {
        const QChar ch = cands[i].ch;
        if (!isOpen(ch) && !isClose(ch))
            continue;

        const QChar mate = mateOf(ch);
        // 扫描上限：防止在畸形文件（比如十万个左括号）里把 UI 卡死
        const int limit = 200000;

        QTextBlock block = doc->findBlock(cands[i].pos);
        if (!block.isValid())
            continue;
        const int offset = cands[i].pos - block.position();

        int matePos = -1;
        if (isOpen(ch)) {
            // 自己是开括号：从自己之后开始找同类型闭括号
            matePos = scanForMate(doc, block, offset + 1, ch, mate, +1, limit);
        } else {
            // 自己是闭括号：从自己之前开始找同类型开括号
            matePos = scanForMate(doc, block, offset, ch, mate, -1, limit);
        }

        if (matePos >= 0) {
            r.valid   = true;
            r.nearPos = cands[i].pos;
            r.matePos = matePos;
            return r;
        }
    }
    return r;
}
