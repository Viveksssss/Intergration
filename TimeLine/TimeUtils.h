#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <QString>
#include <QtGlobal>


inline QString fmtTime(qint64 ms,bool withMs = false){
    if (ms < 0 ){
        ms = 0;
    }
    const qint64 h = ms / 3600000;
    const qint64 m = (ms / 60000)%60;
    const qint64 s = (ms / 1000)%60;

    QString t = h > 0
        ? QString("%1:%2:%3").arg(h)
                             .arg(m,2,10,QChar('0'))
                             .arg(s,2,10,QChar('0'))
        : QString("%1:%2").arg(m).arg(s,2,10,QChar('0'));

    if (withMs){
        t += QString(".%1").arg(ms % 1000,3,10,QChar('0'));
    }
    return t;
}






#endif // TIMEUTILS_H
