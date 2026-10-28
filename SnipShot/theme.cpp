#include "theme.h"

#include <QFontInfo>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

// ===========================================================================
//  字体
// ===========================================================================
QFont Theme::uiFont(int pt, bool bold)
{
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    if (!QFontInfo(f).exactMatch())
        f.setFamily(QStringLiteral("Segoe UI"));
    f.setPointSize(pt);
    f.setBold(bold);
    return f;
}

QFont Theme::monoFont(int pt)
{
    QFont f(QStringLiteral("Cascadia Mono"));
    if (!QFontInfo(f).exactMatch())
        f.setFamily(QStringLiteral("Consolas"));
    f.setStyleHint(QFont::Monospace);
    f.setPointSize(pt);
    return f;
}

// ===========================================================================
//  图标：全部现画。
//
//  为什么要自己画？因为工程里不想放 .qrc + png。
//  做法是「画一张 2 倍图，再把 devicePixelRatio 设成 2」，
//  这样在高 DPI 屏上不会糊，在普通屏上 Qt 会自动缩小一半。
// ===========================================================================
namespace {

QPixmap makeCanvas(int size, qreal dpr)
{
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    return pm;
}

const QColor kIconInk("#dcdce8");

}  // namespace

QIcon Theme::appIcon(int size)
{
    const qreal s = size;
    QPixmap pm = makeCanvas(size, 2.0);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#1d8fe1")));
    p.drawRoundedRect(QRectF(s * 0.04, s * 0.04, s * 0.92, s * 0.92),
                      s * 0.22, s * 0.22);

    p.setPen(QPen(Qt::white, qMax<qreal>(1.4, s * 0.075),
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    const qreal left = s * 0.25;
    const qreal right = s * 0.75;
    const qreal top = s * 0.25;
    const qreal bottom = s * 0.75;
    const qreal arm = s * 0.16;

    p.drawLine(QPointF(left + arm, top), QPointF(left, top));
    p.drawLine(QPointF(left, top), QPointF(left, top + arm));
    p.drawLine(QPointF(right - arm, top), QPointF(right, top));
    p.drawLine(QPointF(right, top), QPointF(right, top + arm));
    p.drawLine(QPointF(left, bottom - arm), QPointF(left, bottom));
    p.drawLine(QPointF(left, bottom), QPointF(left + arm, bottom));
    p.drawLine(QPointF(right, bottom - arm), QPointF(right, bottom));
    p.drawLine(QPointF(right, bottom), QPointF(right - arm, bottom));

    p.setPen(QPen(Qt::white, qMax<qreal>(1.0, s * 0.045),
                  Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(s * 0.42, s * 0.5), QPointF(s * 0.58, s * 0.5));
    p.drawLine(QPointF(s * 0.5, s * 0.42), QPointF(s * 0.5, s * 0.58));

    p.end();
    return QIcon(pm);
}

QIcon Theme::toolIcon(Tool t, int size)
{
    const qreal S = size;
    QPixmap pm = makeCanvas(size, 2.0);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto P = [S](qreal x, qreal y) { return QPointF(x * S, y * S); };

    QPen pen(kIconInk, S * 0.075, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (t) {
    case Tool::Rect:
        p.drawRoundedRect(QRectF(S * 0.18, S * 0.26, S * 0.64, S * 0.48),
                          S * 0.07, S * 0.07);
        break;

    case Tool::Arrow: {
        p.drawLine(P(0.22, 0.78), P(0.62, 0.38));
        QPolygonF head;
        head << P(0.80, 0.20) << P(0.52, 0.26) << P(0.74, 0.48);
        p.setPen(Qt::NoPen);
        p.setBrush(kIconInk);
        p.drawPolygon(head);
        break;
    }

    case Tool::Pen: {
        QPainterPath path;
        path.moveTo(P(0.16, 0.66));
        path.cubicTo(P(0.32, 0.20), P(0.48, 0.86), P(0.64, 0.44));
        path.cubicTo(P(0.72, 0.24), P(0.80, 0.30), P(0.84, 0.38));
        p.drawPath(path);
        break;
    }

    case Tool::Text: {
        QFont f = uiFont(1);
        f.setPixelSize(int(S * 0.86));
        f.setBold(true);
        p.setFont(f);
        p.setPen(kIconInk);
        p.drawText(QRectF(0, 0, S, S * 0.96), Qt::AlignCenter,
                   QStringLiteral("T"));
        break;
    }

    case Tool::Mosaic: {
        const qreal cell = S * 0.17;
        const qreal gap  = S * 0.04;
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                if ((x + y) % 2)
                    continue;
                p.fillRect(QRectF(S * 0.20 + x * (cell + gap),
                                  S * 0.20 + y * (cell + gap),
                                  cell, cell),
                           kIconInk);
            }
        }
        break;
    }
    case Tool::Cursor:{
        QPainterPath cursor;
        cursor.moveTo(S * 0.22, S * 0.10);   // 尖
        cursor.lineTo(S * 0.22, S * 0.72);   // 左边缘竖直向下
        cursor.lineTo(S * 0.36, S * 0.58);   // 内侧凹口
        cursor.lineTo(S * 0.50, S * 0.88);   // 尾巴右下
        cursor.lineTo(S * 0.62, S * 0.82);   // 尾巴右下外侧
        cursor.lineTo(S * 0.48, S * 0.52);   // 回到主体右侧
        cursor.lineTo(S * 0.68, S * 0.52);   // 右侧水平（可选）
        cursor.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(kIconInk);
        p.drawPath(cursor);
        break;
    }

    case Tool::None:
        break;
    }

    p.end();
    return QIcon(pm);
}

QIcon Theme::actionIcon(const char *name, int size)
{
    const qreal S = size;
    QPixmap pm = makeCanvas(size, 2.0);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto P = [S](qreal x, qreal y) { return QPointF(x * S, y * S); };
    const QPen pen(kIconInk, S * 0.075, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QString n = QString::fromLatin1(name);

    if (n == QLatin1String("undo")) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(P(0.34, 0.32));
        path.lineTo(P(0.56, 0.32));
        path.cubicTo(P(0.88, 0.32), P(0.88, 0.72), P(0.56, 0.72));
        path.lineTo(P(0.36, 0.72));
        p.drawPath(path);

        QPolygonF head;
        head << P(0.36, 0.16) << P(0.16, 0.32) << P(0.36, 0.48);
        p.setPen(Qt::NoPen);
        p.setBrush(kIconInk);
        p.drawPolygon(head);

    }
    else if (n == QLatin1String("trash")) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(P(0.18, 0.28), P(0.82, 0.28));        // 盖
        p.drawLine(P(0.38, 0.28), P(0.38, 0.18));        // 提手
        p.drawLine(P(0.38, 0.18), P(0.62, 0.18));
        p.drawLine(P(0.62, 0.18), P(0.62, 0.28));
        QPainterPath body;
        body.moveTo(P(0.26, 0.28));
        body.lineTo(P(0.31, 0.84));
        body.lineTo(P(0.69, 0.84));
        body.lineTo(P(0.74, 0.28));
        p.drawPath(body);

    } else if (n == QLatin1String("copy")) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(S * 0.16, S * 0.16, S * 0.46, S * 0.46),
                          S * 0.08, S * 0.08);
        p.drawRoundedRect(QRectF(S * 0.36, S * 0.36, S * 0.46, S * 0.46),
                          S * 0.08, S * 0.08);

    } else if (n == QLatin1String("save")) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(S * 0.16, S * 0.16, S * 0.68, S * 0.68),
                          S * 0.08, S * 0.08);
        p.drawRect(QRectF(S * 0.32, S * 0.16, S * 0.36, S * 0.24));  // 上部滑块
        p.drawRect(QRectF(S * 0.30, S * 0.56, S * 0.40, S * 0.28));  // 下部纸片

    } else if (n == QLatin1String("close")) {
        p.setPen(pen);
        p.drawLine(P(0.26, 0.26), P(0.74, 0.74));
        p.drawLine(P(0.74, 0.26), P(0.26, 0.74));
    }

    p.end();
    return QIcon(pm);
}
