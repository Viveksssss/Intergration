#include "MultiLineDelegate.h"
#include "BigModel.h"
#include "TableTheme.h"

#include <QPainter>
#include <QTextLayout>
#include <QTextOption>

// ===========================================================================
//  排版:把 text 按 width 排成最多 m_maxLines 行
//
//  QTextLayout 的用法是这样的:
//
//      layout.beginLayout();
//      while (true) {
//          QTextLine line = layout.createLine();
//          if (!line.isValid()) break;        // 文本排完了
//          line.setLineWidth(width);
//          line.setPosition(...);
//      }
//      layout.endLayout();
//
//  注意 createLine() 的语义:**它返回一个有效 line,就说明"还有内容待排"**。
//  它不负责告诉你排完没排完 —— 排完的那一刻 createLine 返回无效值。
//  所有"最多排 N 行"的逻辑都要在循环外面自己数,不能指望 layout。
// ===========================================================================
int MultiLineDelegate::layoutLines(const QFont &font, const QString &text,
                                   int width, QStringList *outLines) const
{
    if (outLines)
        outLines->clear();
    if (text.isEmpty() || width <= 0)
        return 0;

    QTextLayout layout(text, font);
    QTextOption opto;
    opto.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opto);

    layout.beginLayout();
    int produced = 0;
    qreal y = 0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;                          // 文本排完了

        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += line.height();
        ++produced;

        if (outLines)
            outLines->append(layout.text().mid(line.textStart(), line.textLength()));

        //  达到上限就停。**注意停的时候 layout 里还留着没排的文本**,
        //  这正好被 paint 用来判断"要不要画省略号"——看 layout 的
        //  剩余文本量即可,不用自己再算一遍。
        if (produced >= m_maxLines)
            break;
    }
    layout.endLayout();

    return produced;
}

MultiLineDelegate::MultiLineDelegate(int maxLines, QObject *parent)
    : CellBackgroundDelegate(parent)
    , m_maxLines(qMax(1, maxLines))
{
}

// ===========================================================================
//  sizeHint:告诉视图"这一行需要多高"
//
//  这是多行 delegate 唯一的坑,也是最容易漏的一步。
//
//  高度 = 行数 × 行高 + 上下内边距
//
//  行数怎么来?对每一格算一次 layoutLines()。这看起来"很贵"——
//  但要注意:**sizeHint 只为可见行调用**。视图只问屏幕上那 20 行,
//  不会问 100 万次。所以这里再贵也没关系,前提是它真的只在可见范围内调。
//  (验证方法:s7 的自检里数 sizeHint 被调了几次。)
// ===========================================================================
QSize MultiLineDelegate::sizeHint(const QStyleOptionViewItem &opt,
                                  const QModelIndex &index) const
{
    const QString text = index.data(Qt::DisplayRole).toString();

    //  sizeHint 时 opt.rect 还不可靠(视图还没定下来),宽度用不了。
    //  所以这里用一个固定估算宽度算行数——只影响"需要几行"的判断,
    //  真实绘制时 paint 会用准确的 rect 重新排一次。
    //
    //  这是权衡:用真实 width 需要 delegate 知道列宽,而 sizeHint 拿不到。
    //  如果你的行高必须严格跟列宽联动,正确做法是视图 resize 时
    //  调 resizeRowsToContents(),代价是 O(可见行) 的重排。
    const int estWidth = 360;

    QFont f = opt.font;
    f.setPointSize(9);
    const QFontMetrics fm(f);

    const int n = layoutLines(f, text, estWidth - 2 * TableTheme::kCellPadX);
    const int lines = qMax(1, qMin(n, m_maxLines));

    return QSize(-1, lines * fm.lineSpacing() + 2 * TableTheme::kCellPadY);
}

// ===========================================================================
//  measure:纯函数版的"排版",paint 和自检共用
//
//  paint 里那段循环和这里一模一样。写在两个地方会不会漂?
//  会——但只有两份,paint 里的那份是"带绘制副作用"的版本,
//  没法直接复用。折中办法是:把两边**行数/截断判断**都走这里,
//  paint 里只保留绘制。这样"排几行"这件事只有一处定义。
// ===========================================================================
MultiLineDelegate::LayoutResult
MultiLineDelegate::measure(const QFont &font, const QString &text,
                           const QSize &cellSize) const
{
    LayoutResult res;
    const QRect area(QPoint(0, 0),
                     QSize(cellSize.width()  - 2 * TableTheme::kCellPadX,
                           cellSize.height() - 2 * TableTheme::kCellPadY));
    if (text.isEmpty() || area.width() <= 0 || area.height() <= 0)
        return res;

    const QFontMetrics fm(font);
    const qreal lineH = fm.lineSpacing();
    const int canFit = qMax(1, int(area.height() / lineH));

    QTextLayout layout(text, font);
    QTextOption opto;
    opto.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opto);

    layout.beginLayout();
    int drawn = 0;
    qreal y = 0;
    while (drawn < canFit && drawn < m_maxLines) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(area.width());
        line.setPosition(QPointF(0, y));
        y += line.height();
        ++drawn;
    }
    res.lines     = drawn;
    res.truncated = layout.createLine().isValid();
    layout.endLayout();
    return res;
}

// ===========================================================================
//  paint
// ===========================================================================
void MultiLineDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                              const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    const QRect cell = opt.rect;
    const bool selected = opt.state & QStyle::State_Selected;

    // ---- 背景:基类统一处理 ----
    paintBackground(p, opt, index);

    const QString text = index.data(Qt::DisplayRole).toString();
    if (text.isEmpty()) {
        p->restore();
        return;
    }

    QFont f = opt.font;
    f.setPointSize(9);
    p->setFont(f);

    const QFontMetrics fm(f);

    // ---- 可用区域 ----
    //  上下留 TableTheme::kCellPadY,左右留 kCellPadX。
    //  注意这里是**用 opt.rect 算**,不是用 sizeHint 里那个估算宽度。
    const QRect area = cell.adjusted(TableTheme::kCellPadX, TableTheme::kCellPadY,
                                     -TableTheme::kCellPadX, -TableTheme::kCellPadY);

    // ---- 排几行?交给 measure,和自检用同一份判断 ----
    const LayoutResult lr = measure(f, text, cell.size());

    // ---- 用 QTextLayout 再排一遍(这一次带绘制坐标) ----
    QTextLayout layout(text, f);
    QTextOption opto;
    opto.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opto);

    const qreal lineH = fm.lineSpacing();

    layout.beginLayout();
    int drawn = 0;
    qreal y = 0;
    qreal lastLineW = 0;
    while (drawn < lr.lines) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(area.width());
        line.setPosition(QPointF(0, y));
        y += line.height();
        lastLineW = line.naturalTextWidth();
        ++drawn;
    }
    layout.endLayout();

    // ---- 画 ----
    p->setPen(selected ? QColor(Qt::white) : TableTheme::kText);
    const QPointF origin(area.left(), area.top());
    layout.draw(p, origin);

    // ---- 省略号 ----
    //  只在"确实还有内容没排"时画。画在最后一行文字后面,
    //  位置用最后一行的实际宽度推出来——不能拿 area.width() 当起点,
    //  否则省略号会跑到行尾很远的地方。
    if (lr.truncated && drawn > 0) {
        const qreal lastY = area.top() + (drawn - 1) * lineH;
        const qreal exW = fm.horizontalAdvance(QStringLiteral("…"));

        const int left = int(area.width() - lastLineW);
        if (left < int(exW) + 2) {
            //  最后一行已经很满,省略号挤不进去。
            //  退一步:把整个最后一行压成带省略号的短文本重画。
            //  覆盖是安全的——底色刷掉重来,不会和下面的字形叠加。
            const QRect lastRect(area.left(), int(lastY),
                                 area.width(), int(lineH) + 1);
            p->fillRect(lastRect,
                        selected ? TableTheme::kRowSelected
                                 : ((index.row() & 1) ? TableTheme::kRowOdd : TableTheme::kRowEven));
            const QString tail = fm.elidedText(text.right(40), Qt::ElideRight,
                                               area.width());
            p->setPen(selected ? QColor(Qt::white) : TableTheme::kText);
            p->drawText(lastRect, Qt::AlignLeft | Qt::AlignVCenter, tail);
        } else {
            p->setPen(selected ? QColor(Qt::white) : TableTheme::kTextDim);
            p->drawText(QPointF(area.left() + lastLineW, lastY + fm.ascent()),
                        QStringLiteral("…"));
        }
    }

    p->restore();
}
