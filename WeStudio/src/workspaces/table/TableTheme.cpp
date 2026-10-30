#include "TableTheme.h"

#include <QFontInfo>

// ===========================================================================
//  字体：优先用系统里真实存在的中文字体
//
//  QFont 的构造函数**不会失败**——字体不存在时它会静默替换成别的字体,
//  所以必须用 QFontInfo::exactMatch() 回头确认一遍。
//  不做这一步,换一台机器中文就会变成宋体/方块。
// ===========================================================================
QFont TableTheme::uiFont(int pt, bool bold)
{
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    if (!QFontInfo(f).exactMatch())
        f.setFamily(QStringLiteral("Segoe UI"));
    f.setPointSize(pt);
    f.setBold(bold);
    return f;
}

QFont TableTheme::monoFont(int pt)
{
    QFont f(QStringLiteral("Cascadia Mono"));
    if (!QFontInfo(f).exactMatch())
        f.setFamily(QStringLiteral("Consolas"));
    f.setStyleHint(QFont::Monospace);
    f.setPointSize(pt);
    return f;
}
