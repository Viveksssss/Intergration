// ============================================================================
//  TimeUtils.h —— 时间格式化
// ----------------------------------------------------------------------------
//  ms -> "m:ss" / "h:mm:ss"，可选带毫秒。
//  写成 inline 而不是 .cpp + .h，是因为它只有 10 行、被两个类都要用，
//  为它单独开一个编译单元反而更啰嗦。
// ============================================================================
#pragma once

#include <QString>
#include <QtGlobal>

inline QString fmtTime(qint64 ms, bool withMs = false)
{
    if (ms < 0) ms = 0;

    const qint64 h = ms / 3600000;
    const qint64 m = (ms / 60000) % 60;
    const qint64 s = (ms / 1000) % 60;

    QString t = h > 0
        ? QString("%1:%2:%3").arg(h)
                             .arg(m, 2, 10, QChar('0'))
                             .arg(s, 2, 10, QChar('0'))
        : QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));

    if (withMs)
        t += QString(".%1").arg(ms % 1000, 3, 10, QChar('0'));   // 补够 3 位

    return t;
}
