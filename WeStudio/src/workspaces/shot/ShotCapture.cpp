#include "ShotCapture.h"

#include "ShotTheme.h"

#include <QGuiApplication>
#include <QLinearGradient>
#include <QStringList>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>

namespace {

// ---------------------------------------------------------------------------
//  箭头：一根杆 + 一个三角形头
//
//  杆要画到"三角形底部"而不是尖端，否则线宽大于 1 时线头会从尖端钻出来，
//  看起来像箭头被啃掉一小块。
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
//  马赛克 = 先缩小，再用「最近邻」放大
//
//  三个细节决定成败：
//   1. 缩小必须 FastTransformation（否则会先糊一次，块边界不清）
//   2. 放大也必须关掉 SmoothPixmapTransform（否则块之间被插值成渐变）
//   3. copy() 收的是设备像素，所以 rect 要先乘 dpr
// ---------------------------------------------------------------------------
void drawMosaic(QPainter &p, const QImage &shot, const QRectF &rect, int block)
{
    if (block < 2)
        block = 2;

    const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1.0;

    QRect srcPx(qRound(rect.x() * dpr), qRound(rect.y() * dpr),
                qRound(rect.width() * dpr), qRound(rect.height() * dpr));
    srcPx = srcPx.intersected(QRect(QPoint(0, 0), shot.size()));
    if (srcPx.width() < 2 || srcPx.height() < 2)
        return;

    const int blockPx = qMax(2, qRound(block * dpr));
    const int bw = qMax(1, srcPx.width()  / blockPx);
    const int bh = qMax(1, srcPx.height() / blockPx);

    const QImage tile = shot.copy(srcPx);
    const QImage tiny = tile.scaled(bw, bh, Qt::IgnoreAspectRatio,
                                    Qt::FastTransformation);

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(rect, tiny);       // 目标用逻辑坐标，Qt 自己乘 dpr
    p.restore();
}

}  // namespace

// ===========================================================================
//  抓屏
// ===========================================================================
QRect ShotCapture::virtualGeometry()
{
    QRect r;
    for (QScreen *s : QGuiApplication::screens())
        r = r.united(s->geometry());
    return r;
}

QImage ShotCapture::grabAll()
{
    const QRect virt = virtualGeometry();
    if (virt.isEmpty())
        return QImage();

    // 多屏 dpr 可能不同，取最大的那个，保证不会"降清晰度"
    qreal dpr = 1.0;
    for (QScreen *s : QGuiApplication::screens())
        dpr = qMax(dpr, s->devicePixelRatio());

    QImage canvas(QSize(qRound(virt.width() * dpr), qRound(virt.height() * dpr)),
                  QImage::Format_RGB32);
    canvas.fill(Qt::black);
    canvas.setDevicePixelRatio(dpr);   // 之后这张图就"按逻辑坐标"使用

    QPainter p(&canvas);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    for (QScreen *s : QGuiApplication::screens()) {
        const QPixmap pm = s->grabWindow(0);
        if (pm.isNull())
            continue;

        QImage img = pm.toImage();     // img.size() 是物理像素
        img.setDevicePixelRatio(dpr);

        // 目标矩形用逻辑坐标；源矩形用像素坐标。两者都写全，避免依赖 dpr 推断
        const QRectF target(QPointF(s->geometry().topLeft() - virt.topLeft()),
                            QSizeF(s->geometry().size()));
        p.drawImage(target, img, QRectF(0, 0, img.width(), img.height()));
    }
    p.end();
    return canvas;
}

// ===========================================================================
//  假桌面：自检截图用，画三个窗口 + 一张柱状图，细节足够看清马赛克效果
// ===========================================================================
QImage ShotCapture::makeTestDesktop()
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

        p.setBrush(accent);
        p.drawRoundedRect(QRectF(r.x(), r.y(), r.width(), 40), 12, 12);
        p.drawRect(QRectF(r.x(), r.y() + 28, r.width(), 12));

        const QColor dots[3] = { QColor("#ff5f57"), QColor("#febc2e"), QColor("#28c840") };
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            p.setPen(QPen(QColor(0, 0, 0, 60), 2));
            p.drawEllipse(QPointF(r.x() + 22 + i * 20, r.y() + 20), 5, 5);
        }

        QFont f = ShotTheme::uiFont(1, true);
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
        QFont f = ShotTheme::monoFont(1);
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
        QFont f = ShotTheme::uiFont(1);
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
        QFont f = ShotTheme::uiFont(1);
        f.setPixelSize(14);
        p.setFont(f);
        p.drawText(QRectF(chartRect.x() + 40, base + 8, 400, 24),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Mon  Tue  Wed  Thu  Fri  Sat  Sun"));
    }

    p.end();
    return img;
}

// ===========================================================================
//  画一个标注
// ===========================================================================
void ShotCapture::paintOne(QPainter &p, const QImage &shot, const Annotation &a)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    switch (a.tool) {
    case Tool::Rect: {
        p.setPen(QPen(a.color, a.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(a.rect.normalized(), 2, 2);
        break;
    }

    case Tool::Arrow:
        drawArrow(p, a.p1, a.p2, a.color, a.width);
        break;

    case Tool::Pen:
        p.setPen(QPen(a.color, a.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(a.path);
        break;

    case Tool::Text: {
        QFont f = ShotTheme::uiFont(1, true);
        f.setPixelSize(a.fontSize);

        const QFontMetricsF fm(f);
        QPainterPath tp;
        tp.addText(0, fm.ascent(), f, a.text);

        p.translate(a.textPos);

        // 先描一圈半透明黑边：不管文字压在多亮的背景上都读得清
        p.setPen(QPen(QColor(0, 0, 0, 170), qMax(2.0, a.fontSize * 0.14),
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(tp);

        p.setPen(Qt::NoPen);
        p.fillPath(tp, a.color);
        break;
    }

    case Tool::Mosaic:
        drawMosaic(p, shot, a.rect.normalized(), a.block);
        break;

    case Tool::None:
        break;
    }

    p.restore();
}

// ===========================================================================
//  导出：裁剪 + 重画标注
//
//  为什么要在导出的图上重画，而不是直接截取屏幕上那块？
//   因为屏幕上那块还盖着遮罩、边框、手柄、工具栏，而且图上没有标注的
//   "干净原版"。手里有原始截图，重画一遍是最干净的。
// ===========================================================================
QImage ShotCapture::renderRegion(const QImage &shot, const QRect &sel,
                                 const QVector<Annotation> &anns)
{
    if (shot.isNull() || !sel.isValid() || sel.isEmpty())
        return QImage();

    const qreal dpr = shot.devicePixelRatio() > 0 ? shot.devicePixelRatio() : 1.0;

    QRect pxRect(qRound(sel.x() * dpr), qRound(sel.y() * dpr),
                 qRound(sel.width() * dpr), qRound(sel.height() * dpr));
    pxRect = pxRect.intersected(QRect(QPoint(0, 0), shot.size()));
    if (pxRect.isEmpty())
        return QImage();

    QImage out = shot.copy(pxRect);
    out.setDevicePixelRatio(dpr);      // 保持 dpr，这样下面还能用逻辑坐标画

    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(-sel.topLeft());       // 逻辑坐标 → 以选区左上角为原点

    for (const Annotation &a : anns)
        paintOne(p, shot, a);

    p.end();
    return out;
}
