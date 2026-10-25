#ifndef BRACKETMATCHER_H
#define BRACKETMATCHER_H

#include <qobject.h>
class QTextDocument;
class BracketMatcher
{
public:

    struct Result{
        bool valid = false;
        int nearPos = -1;  //光标旁边的括号的绝对位置
        int matePos = -1;  // 配对括号
        bool mastered = false;  // 暂不适用
    };

    static Result match(QTextDocument*doc,int cursorPos);
    static QChar mateOf(QChar ch);
    static bool isOpen(QChar ch);
    static bool isClose(QChar ch);
};

#endif // BRACKETMATCHER_H
