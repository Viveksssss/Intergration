#include "CellBackgroundDelegate.h"
#include "TableTheme.h"

#include <QPainter>

CellBackgroundDelegate::CellBackgroundDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

// ===========================================================================
//  背景:选中 > 斑马纹
//
//  顺序很重要。先判断 selected 再判断奇数行:一个被选中的奇数行
//  应该是"选中色",不是"斑马纹的奇色"。写反了(或者用两个独立 if)
//  就会出现选中行一半是斑马纹一半是选中色。
//
//  用 else if 而不是两个 if —— 这类"互斥的样式状态"一律用 else if,
//  让编译器（和读代码的人）都不用去想"两个都成立怎么办"。
// ===========================================================================
void CellBackgroundDelegate::paintBackground(QPainter *p,
                                             const QStyleOptionViewItem &opt,
                                             const QModelIndex &index)
{
    const QRect cell = opt.rect;
    const bool selected = opt.state & QStyle::State_Selected;

    if (selected)
        p->fillRect(cell, TableTheme::kRowSelected);
    else if (index.row() & 1)
        p->fillRect(cell, TableTheme::kRowOdd);
    else
        p->fillRect(cell, TableTheme::kRowEven);

    //  右分隔线。上下各缩 2px,让线段不顶到行边界——
    //  相邻两行的竖线连起来会显得"格子被框死了",留 2px 反而清楚。
    p->setPen(TableTheme::kGridLine);
    p->drawLine(cell.right(), cell.top() + 2, cell.right(), cell.bottom() - 2);
}

// ===========================================================================
//  paint:背景自己画,文字交给基类
//
//  QStyledItemDelegate::paint 会在给定 rect 里画文字,并且会**先画它自己的
//  背景**（用 palette/selection）。这个"它自己的背景"就是白底问题的来源之一。
//
//  干净的解法:构造一个临时的 QStyleOptionViewItem,把 state 里的
//  State_Selected 和 State_HasFocus 清掉,再交给基类。这样基类
//  会画"普通状态"的背景（跟着调色板走,在我们这套深色 QSS 下是透明的),
//  然后正常画文字。
//
//  另一个办法是调 initStyleOption 之后手动画文字（QStyle::drawItemText）。
//  这里选前者:改动小,而且文字的对齐/换行仍由 Qt 负责。
// ===========================================================================
void CellBackgroundDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                                  const QModelIndex &index) const
{
    p->save();
    paintBackground(p, opt, index);
    p->restore();

    QStyleOptionViewItem o = opt;
    //  清掉"选中"和"焦点",让基类不要再去画选中高亮。
    o.state &= ~QStyle::State_Selected;
    o.state &= ~QStyle::State_HasFocus;
    //  背景已经画过了,让基类不要再刷一遍。
    o.backgroundBrush = QBrush(Qt::NoBrush);

    QStyledItemDelegate::paint(p, o, index);
}
