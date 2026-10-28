#include "shotcapture.h"
#include "Theme.h"

#include <QGuiApplication>
#include <QLinearGradient>
#include <QStringList>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>



namespace ShotCapture{


/* 逻辑窗口 本机是1707，960,实际2560，1440，有1.5被缩放 */
QRect virtualGeometry()
{
    QScreen*screen = QGuiApplication::primaryScreen();
    return screen?screen->geometry() : QRect();
}

/* 截图屏幕 */
QImage grabAll()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QImage();
    const QPixmap pm = screen->grabWindow(0);
    if (pm.isNull())
        return QImage();
    QImage img = pm.toImage();
    // 本机devidePixelRatio是1.5
    img.setDevicePixelRatio(screen->devicePixelRatio());
    return img;
}

/* 假桌面，自检截图使用，绘制三个窗口*/
QImage makeTestDesktop()
{
    const QRect virt = virtualGeometry();
    qreal dpr = 1.0;
    for (QScreen *s : QGuiApplication::screens())
        dpr = qMax(dpr, s->devicePixelRatio());

    QImage img(QSize(qRound(virt.width() * dpr), qRound(virt.height() * dpr)),
               QImage::Format_RGB32);
    img.setDevicePixelRatio(dpr);

    const qreal W = virt.width();
    const qreal H = virt.height();

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // ---- 背景渐变 + 细网格
    QLinearGradient bg(0, 0, W, H);
    bg.setColorAt(0.0, QColor("#1a2130"));
    bg.setColorAt(1.0, QColor("#2c1d2b"));
    p.fillRect(QRectF(0, 0, W, H), bg);

    p.setPen(QPen(QColor(255, 255, 255, 16), 1));
    for (int x = 0; x < int(W); x += 48)
        p.drawLine(QPointF(x, 0), QPointF(x, H));
    for (int y = 0; y < int(H); y += 48)
        p.drawLine(QPointF(0, y), QPointF(W, y));

    // ---- 一个"窗口"：阴影 + 圆角 + 彩色标题栏 + 三个圆点
    auto card = [&p](const QRectF &r, const QColor &accent, const QString &title) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 70));
        p.drawRoundedRect(r.translated(0, 10), 12, 12);

        p.setBrush(QColor("#12161f"));
        p.drawRoundedRect(r, 12, 12);

        p.save();

        QPainterPath clip;
        clip.addRoundedRect(r, 12, 12);
        p.setClipPath(clip);

        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        p.drawRect(QRectF(r.x(), r.y(), r.width(), 40));

        p.restore();
        /*
            p.setBrush(accent);
            p.drawRoundedRect(QRectF(r.x(), r.y(), r.width(), 40), 12, 12);
            p.drawRect(QRectF(r.x(), r.y() + 28, r.width(), 12));
        */
        const QColor dots[3] = { QColor("#ff5f57"), QColor("#febc2e"), QColor("#28c840") };
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            p.setPen(QPen(QColor(0, 0, 0, 60), 2));
            p.drawEllipse(QPointF(r.x() + 22 + i * 20, r.y() + 20), 5, 5);
        }

        QFont f = Theme::uiFont(1, true);
        f.setPixelSize(15);
        p.setFont(f);
        p.setPen(QColor("#0d1017"));
        p.drawText(QRectF(r.x() + 90, r.y(), r.width() - 110, 40),
                   Qt::AlignVCenter | Qt::AlignLeft, title);
    };

    // ---- 窗口 1：代码
    const QRectF codeRect(120, 100, 700, 620);
    card(codeRect, QColor("#3f7fbf"), QStringLiteral("main.cpp"));

    {
        QFont f = Theme::monoFont(1);
        f.setPixelSize(17);
        p.setFont(f);

        // 当前行高亮
        p.fillRect(QRectF(codeRect.x() + 8, codeRect.y() + 152,
                          codeRect.width() - 16, 26),
                   QColor(46, 168, 255, 26));

        const QStringList lines = {
                                   QStringLiteral("#include <QApplication>"),
                                   QStringLiteral("#include \"SnippingOverlay.h\""),
                                   QString(),
                                   QStringLiteral("int main(int argc, char **argv)"),
                                   QStringLiteral("{"),
                                   QStringLiteral("    QApplication app(argc, argv);"),
                                   QStringLiteral("    QImage shot = ShotCapture::grabAll();"),
                                   QStringLiteral("    auto *overlay = new SnippingOverlay(shot);"),
                                   QStringLiteral("    overlay->show();"),
                                   QStringLiteral("    return app.exec();"),
                                   QStringLiteral("}"),
                                   };

        qreal y = codeRect.y() + 66;
        for (int i = 0; i < lines.size(); ++i) {
            p.setPen(QColor(110, 116, 134));
            p.drawText(QRectF(codeRect.x() + 16, y, 34, 26),
                       Qt::AlignRight | Qt::AlignVCenter, QString::number(i + 1));

            p.setPen(i == 5 ? QColor("#e8e8f0") : QColor("#9fd0a8"));
            p.drawText(QRectF(codeRect.x() + 62, y, codeRect.width() - 80, 26),
                       Qt::AlignVCenter | Qt::AlignLeft, lines[i]);
            y += 26;
        }
    }

    // ---- 窗口 2：聊天
    const QRectF chatRect(880, 100, 700, 380);
    card(chatRect, QColor("#8a5fbf"), QStringLiteral("讨论"));

    {
        QFont f = Theme::uiFont(1);
        f.setPixelSize(16);
        p.setFont(f);

        const QString bubbles[3] = {
            QStringLiteral("遮罩要在选区位置挖个洞，"),
            QStringLiteral("不然截图里的内容也是暗的。"),
            QStringLiteral("even-odd 填充规则最省事。")
        };
        qreal y = chatRect.y() + 62;
        for (int i = 0; i < 3; ++i) {
            const bool right = (i % 2 == 1);
            const qreal w = chatRect.width() * 0.56;
            const QRectF r = right
                                 ? QRectF(chatRect.right() - w - 24, y, w, 44)
                                 : QRectF(chatRect.x() + 24, y, w, 44);

            p.setPen(Qt::NoPen);
            p.setBrush(right ? QColor("#2ea8ff") : QColor("#262c3a"));
            p.drawRoundedRect(r, 10, 10);

            p.setPen(right ? QColor("#0b0b10") : QColor("#d8dce8"));
            p.drawText(r.adjusted(16, 0, -16, 0), Qt::AlignVCenter | Qt::AlignLeft, bubbles[i]);
            y += 58;
        }
    }

    // ---- 窗口 3：柱状图（马赛克就盖在这块上）
    const QRectF chartRect(880, 520, 700, 330);
    card(chartRect, QColor("#3fae8f"), QStringLiteral("每周提交"));

    {
        const int vals[9] = { 42, 68, 35, 90, 55, 72, 30, 84, 62 };
        const qreal base = chartRect.bottom() - 54;
        const qreal bw = 46;
        for (int i = 0; i < 9; ++i) {
            const qreal h = vals[i] * 1.9;
            const QRectF r(chartRect.x() + 40 + i * 72, base - h, bw, h);
            p.setPen(Qt::NoPen);
            p.setBrush(i % 2 ? QColor("#5fd0b0") : QColor("#3fae8f"));
            p.drawRoundedRect(r, 6, 6);
        }

        p.setPen(QColor("#8a92a6"));
        QFont f = Theme::uiFont(1);
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(QRectF(chartRect.x() + 40, base + 8, 400, 24),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Mon  Tue  Wed  Thu  Fri  Sat  Sun"));
    }

    p.end();
    return img;
}

void drawArrow(QPainter &p, const QPointF &tail, const QPointF &head,
               const QColor &c, int w)
{
    const QLineF line(tail, head);
    const qreal len = line.length();
    if (len < 1.0)
        return;

    // 头长随线长增长，但有上下限，太短太长的箭头都难看
    const qreal headLen  = qBound(10.0, len * 0.32, 22.0 + w * 2.0);
    const qreal headHalf = headLen * 0.46;

    const QPointF dir = (head - tail) / len;          // 单位方向
    const QPointF base = head - dir * headLen;        // 三角形底边中点

    p.setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawLine(tail, base);

    const QPointF n(-dir.y(), dir.x());               // 法向量
    QPolygonF tri;
    tri << head
        << (base + n * headHalf)
        << (base - n * headHalf);

    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPolygon(tri);
}

void drawMosaic(QPainter&p,const QImage&shot,const QRectF&rect,int block){
    if (block < 2){
        block = 2;
    }

    const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1.0;
    QRect srcPx(qRound(rect.x() * dpr),qRound(rect.y()*dpr),qRound(rect.width()*dpr),qRound(rect.height()*dpr));
    srcPx = srcPx.intersected(QRect(QPoint(0,0),shot.size()));
    if (srcPx.width() < 2 || srcPx.height() < 2){
        return;
    }

    const int blockPx = qMax(2,qRound(block*dpr));
    const int bw = qMax(1,srcPx.width() / blockPx);
    const int bh = qMax(1,srcPx.height() / blockPx);

    const QImage tile = shot.copy(srcPx);
    const QImage tiny = tile.scaled(bw, bh, Qt::IgnoreAspectRatio,
                                    Qt::FastTransformation);

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(rect, tiny);       // 目标用逻辑坐标，Qt 自己乘 dpr
    p.restore();
}

void paintOne(QPainter &p, const QImage &shot, const Annotation &a)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing,true);

    switch(a.tool){
    case Tool::None:{
        break;
    }
    case Tool::Rect:{
        p.setPen(QPen(a.color,a.width,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(a.rect.normalized(),2,2);
        break;
    }
    case Tool::Arrow:{
        drawArrow(p,a.p1,a.p2,a.color,a.width);
        break;
    }
    case Tool::Pen:{
        p.setPen(QPen(a.color, a.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(a.path);
        break;
    }
    case Tool::Mosaic:{
        drawMosaic(p,shot,a.rect.normalized(),a.block);
        break;
    }
    case Tool::Text:{
        QFont f = Theme::uiFont(1,true);
        f.setPixelSize(a.fontSize);

        const QFontMetrics fm(f);
        QPainterPath path;
        path.addText(0,fm.ascent(),f,a.text);
        p.translate(a.textPos);

        p.setPen(QPen(QColor(0, 0, 0, 170), qMax(2.0, a.fontSize * 0.14),
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        p.setPen(Qt::NoPen);
        p.fillPath(path,a.color);
        break;
    }
    default : {
        break;
    }
    }

    p.restore();
}

QImage renderRegion(const QImage &shot, const QRect &sel, const QVector<Annotation> &anns)
{
    if (shot.isNull() || !sel.isValid() || sel.isEmpty())
        return QImage();
    const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1.0;

    QRect pxRect(qRound(sel.x()*dpr),qRound(sel.y()*dpr),qRound(sel.width()*dpr),qRound(sel.height()*dpr));
    pxRect = pxRect.intersected(QRect(QPoint(0,0),shot.size()));
    if (pxRect.isEmpty()){
        return QImage();
    }

    QImage out = shot.copy(pxRect);
    out.setDevicePixelRatio(dpr);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.translate(-sel.topLeft());
    for(const Annotation&a:anns){
        paintOne(p,shot,a);
    }
    p.end();
    return out;
}



}
