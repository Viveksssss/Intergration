#include "ProgressDelegate.h"
#include "BigModel.h"
#include "TableTheme.h"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>

// ===========================================================================
//  颜色映射
//
//  这里刻意把"状态"优先级排在"进度"前面:一个「失败」的任务进度条
//  哪怕已经走到 85%,也应该是红的——因为用户第一眼要看到的是"出事了",
//  而不是"快好了"。
// ===========================================================================
QColor ProgressDelegate::colorFor(int progress, int status)
{
    if (status == BigModel::Failed)
        return TableTheme::kRed;
    if (status == BigModel::Done || progress >= 100)
        return TableTheme::kGreen;
    if (status == BigModel::Queued)
        return TableTheme::kTextDim;          // 灰:还没开始
    return TableTheme::kAccent;               // 蓝:进行中
}

ProgressDelegate::ProgressDelegate(QObject *parent)
    : CellBackgroundDelegate(parent)
{
}

QSize ProgressDelegate::sizeHint(const QStyleOptionViewItem &opt,
                                 const QModelIndex &index) const
{
    //  明确给出高度,免得视图为"这一格多高"去量文字。
    //  宽度给 opt.rect 的宽度没有意义(sizeHint 时 rect 还没定),
    //  所以给 -1,让视图用列宽。
    Q_UNUSED(index);
    QFont f = opt.font;
    f.setPointSize(9);
    const QFontMetrics fm(f);
    return QSize(-1, qMax(TableTheme::kDefaultRowH, fm.height() + 2 * TableTheme::kCellPadY));
}

// ===========================================================================
//  paint
//
//  整个函数只有三步:抽进度值 → 算几何 → 画。没有一行依赖"第几行",
//  所以无论是第 1 行还是第 999999 行,耗时完全一样。
// ===========================================================================
void ProgressDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                             const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    // ---- 1. 背景 ----
    //  一行解决。以前这里抄了 12 行选中/斑马纹/分隔线,
    //  和另外两个 delegate 一模一样。抽到基类之后,改主题只改一处。
    paintBackground(p, opt, index);

    const bool selected = opt.state & QStyle::State_Selected;
    const QRect cell = opt.rect;

    // ---- 2. 取数据 ----
    //  走自定义 role,一次拿全。**不要**解析 DisplayRole 的字符串,
    //  也**不要** index.sibling(行, 状态列).data() 去问隔壁列——
    //  后者会让每一格多一次完整的 data() 调用,虚拟化省下来的开销
    //  又被吃掉一半。
    int value  = index.data(BigModel::ProgressRole).toInt();
    int status = index.data(BigModel::StatusRole).toInt();
    value = qBound(0, value, 100);

    // ---- 3. 计算进度条几何 ----
    //  进度条只占格子中间一条,上下留白。文字画在条上,不占条外空间,
    //  这样行高可以做得很矮(s1 里试过用"文字在条下方",行高必须 44)。
    QRect bar = cell.adjusted(TableTheme::kCellPadX, 0, -TableTheme::kCellPadX, 0);
    const int barH = TableTheme::kBarHeight;
    bar.setTop(cell.center().y() - barH / 2);
    bar.setHeight(barH);

    //  太窄就不画条,退化成纯文字。窗口拖到很小时列宽可能小于 40px,
    //  这时候硬画条会得到一个 0 宽或被裁掉的圆角,很难看。
    if (bar.width() >= 40) {
        //  底槽:半透明白,深色表里比任何实色都自然
        p->setPen(Qt::NoPen);
        p->setBrush(TableTheme::kTrack);
        p->drawRoundedRect(bar, TableTheme::kBarRadius, TableTheme::kBarRadius);

        //  已完成部分
        const int fillW = bar.width() * value / 100;
        if (fillW > 0) {
            QRect fill = bar;
            fill.setWidth(fillW);
            //  极端情况下(1% 宽)圆角半径大于宽度,圆角会画成奇怪的形状,
            //  所以半径要跟着宽度收。
            const int r = qMin(TableTheme::kBarRadius, fillW / 2);
            p->setBrush(colorFor(value, status));
            QPainterPath path;
            path.addRoundedRect(fill, r, r);
            p->drawPath(path);
        }

        //  文字叠在条的右侧外面,而不是压在条上。
        //  压在条上要处理"白字压在浅绿上"的对比度问题,移到条外省事得多。
        QFont f = opt.font;
        f.setPointSize(9);
        p->setFont(f);
        p->setPen(selected ? Qt::white : TableTheme::kText);
        const QRect txtRect = cell.adjusted(TableTheme::kCellPadX, 0,
                                            -TableTheme::kCellPadX, 0);
        p->drawText(txtRect, Qt::AlignRight | Qt::AlignVCenter,
                    QString::number(value) + QLatin1Char('%'));
    } else {
        //  退化路径:只画百分比文字
        QFont f = opt.font;
        f.setPointSize(9);
        p->setFont(f);
        p->setPen(selected ? Qt::white : TableTheme::kText);
        p->drawText(cell, Qt::AlignCenter, QString::number(value) + QLatin1Char('%'));
    }

    p->restore();
}
