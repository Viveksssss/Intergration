#include "bracketmatcher.h"

#include <QTextBlock>
#include <QTextDocument>

namespace {

const QString kOpen = QStringLiteral("([{");
const QString kClose = QStringLiteral(")]}");
const QString kAll = QStringLiteral("(){}[]");


int scanForMate(QTextDocument *doc, QTextBlock block, int offsetInBlock,
                QChar open, QChar close, int direction, int limit)
{
    int depth = 1;
    int steps = 0;
    while(block.isValid()){
        const QString text = block.text();
        int i = direction > 0 ? offsetInBlock : qMin(offsetInBlock,text.size()) - 1;
        while(i >= 0 && i < text.size()){
            const QChar ch = text.at(i);
            if (ch == open){
                ++depth;
            }else if (ch == close){
                --depth;
                if (depth == 0){
                    return block.position() + i;
                }
            }
            if (++steps > limit){
                return -1;
            }
            i+=direction;
        }
        //         0 1 2 3
        // 举例 比如 ( a )
        //          0 1 2
        /*
            块内偏移和字符索引不一样，块内偏移可以有size+1个位置。
            如果偏移是k，那么左侧字符索引就是k-1，右侧字符索引就是k
            direction呢根据方向选择-1/1
                                                        0 1 2
            调用方传入的offsetInBlock对应的是括号的索引，比如 a b c
            如此看来，假如a是左括号，那么传入的偏移量就是0，因为块内偏移和右侧字符的索引一致
            那么这时候，根据direction的正负1数值，如果向右匹配+1，就到了b的位置，不会重复计算a的位置正确；
            同理如果b是左括号，向左匹配-1，也就是0，从a开始往左计算也正确。
        */

        block = (direction > 0 ) ? block.next() : block.previous();
        if (block.isValid()){
            offsetInBlock = (direction > 0) ? 0 : block.text().size();
        }
    }
    return -1;
}



}



BracketMatcher::Result BracketMatcher::match(QTextDocument *doc, int cursorPos)
{
    Result r;
    if (!doc || cursorPos < 0 ){
        return r;
    }

    const int docLen = doc->characterCount()- 1;
    if (cursorPos > docLen){
        return r;
    }

    struct Cand {
        int pos;
        QChar ch;
    };
    Cand cands[2];
    int n = 0;
    // 左侧候选
    if (cursorPos - 1 >= 0 && cursorPos - 1 < docLen){
        QTextCursor c(doc);
        c.setPosition(cursorPos - 1);
        c.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
        const QString s = c.selectedText();
        if (!s.isEmpty()){
            cands[n++] = {
                cursorPos -1,s.at(0)
            };
        }
    }
    // 右侧候选
    if (cursorPos < docLen){
        QTextCursor c(doc);
        c.setPosition(cursorPos);
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        const QString s = c.selectedText();
        if (!s.isEmpty())
            cands[n++] = { cursorPos, s.at(0) };
    }
    for(int i = 0 ; i < n ; ++i){
        const QChar ch = cands[i].ch;
        if (!isOpen(ch) && !isClose(ch)){
            continue;
        }
        const QChar mate = mateOf(ch);
        const int limit = 200000;

        QTextBlock block = doc->findBlock(cands[i].pos);
        if (!block.isValid()){
            continue;
        }
        // 块内偏移
        const int offset = cands[i].pos - block.position();
        int matePos = -1;
        if (isOpen(ch)){
            matePos = scanForMate(doc,block,offset+1,ch,mate,+1,limit);
        }else{
            matePos = scanForMate(doc,block,offset,ch,mate,-1,limit);
        }

        if (matePos >= 0){
            r.valid = true;
            r.nearPos = cands[i].pos;
            r.matePos = matePos;
            return r;
        }
    }
    return r;
}

QChar BracketMatcher::mateOf(QChar ch)
{
    const int idx = kAll.indexOf(ch);
    if (idx< 0 )return QChar();
    return kAll.at(idx^1); // kAll 的排列是 "()[]{}"，同一对的下标是 i^1（0<->1, 2<->3, 4<->5）
}

bool BracketMatcher::isOpen(QChar ch)
{
    return kOpen.contains(ch);
}

bool BracketMatcher::isClose(QChar ch)
{
    return kClose.contains(ch);
}
