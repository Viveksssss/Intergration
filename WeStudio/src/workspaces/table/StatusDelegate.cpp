#include "StatusDelegate.h"
#include "BigModel.h"
#include "TableTheme.h"

#include <QPainter>

QColor StatusDelegate::dotColor(int status)
{
    switch (status) {
    case BigModel::Done:    return TableTheme::kGreen;
    case BigModel::Running: return TableTheme::kAccent;
    case BigModel::Failed:  return TableTheme::kRed;
    default:                return TableTheme::kTextDim;
    }
}

StatusDelegate::StatusDelegate(QObject *parent)
    : CellBackgroundDelegate(parent)
{
}

QSize StatusDelegate::sizeHint(const QStyleOptionViewItem &opt,
                               const QModelIndex &index) const
{
    Q_UNUSED(index);
    const QFontMetrics fm(opt.font);
    return QSize(-1, qMax(TableTheme::kDefaultRowH,
                          fm.height() + 2 * TableTheme::kCellPadY));
}

void StatusDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                           const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    const QRect cell = opt.rect;
    const bool selected = opt.state & QStyle::State_Selected;

    //  背景、斑马纹、右分隔线 —— 走基类,和另外两个 delegate 共用一份实现。
    paintBackground(p, opt, index);

    // ---- 圆点 ----
    const int st = index.data(BigModel::StatusRole).toInt();
    const int d = 8;                            // 直径
    const QRect dot(cell.left() + TableTheme::kCellPadX,
                    cell.center().y() - d / 2, d, d);
    p->setPen(Qt::NoPen);
    p->setBrush(dotColor(st));
    p->drawEllipse(dot);

    //  进行中的状态加一圈淡淡的光晕,让人一眼看出"这个还在动"。
    //  用半透明同色圆环,不用动画——表格里每格都跑动画会拖垮滚动。
    if (st == BigModel::Running) {
        QColor halo = dotColor(st);
        halo.setAlpha(60);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(halo, 2));
        p->drawEllipse(dot.adjusted(-3, -3, 3, 3));
    }

    // ---- 文字 ----
    //  圆点后面留 6px 间距再排字。用同一套 font metrics 保证垂直居中。
    QFont f = opt.font;
    f.setPointSize(9);
    p->setFont(f);

    QRect txt = cell.adjusted(TableTheme::kCellPadX + d + 8, 0,
                              -TableTheme::kCellPadX, 0);
    p->setPen(selected ? QColor(Qt::white) : TableTheme::kText);
    p->drawText(txt, Qt::AlignLeft | Qt::AlignVCenter,
                index.data(Qt::DisplayRole).toString());

    p->restore();
}
