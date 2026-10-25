#include "bracketmatcher.h"

#include <QTextDocument>

namespace {

const QString kOpen = QStringLiteral("([{");
const QString kClose = QStringLiteral(")]}");
const QString kALL = QStringLiteral("(){}[]");

}


BracketMatcher::Result BracketMatcher::match(QTextDocument *doc, int cursorPos)
{

}

QChar BracketMatcher::mateOf(QChar ch)
{

}

bool BracketMatcher::isOpen(QChar ch)
{

}

bool BracketMatcher::isClose(QChar ch)
{

}
