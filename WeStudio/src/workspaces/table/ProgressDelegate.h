#pragma once

#include "CellBackgroundDelegate.h"

// ===========================================================================
//  ProgressDelegate —— 在表格格子里画一根进度条。
//
//  有三个层次的写法,难度和可控性递增:
//
//  1. 用 QStyleOptionProgressBar 交给 style 画
//     最省事,但**跟随系统主题**:浅色系统下是浅色条,在这张深色表里
//     会像一块白斑。想改颜色就得碰 palette,而 palette 是全局的。
//
//  2. 用 QProgressBar 控件 + setIndexWidget
//     100 万行 × 一个控件 = 灾难。视图一滚动就得创建/销毁控件,
//     而且 setIndexWidget 不参与虚拟化,内存直接爆。
//
//  3. 自己 paintEvent 画(本文件采用)
//     QPainter 画两个圆角矩形:底槽 + 已完成的填充。
//     颜色、圆角、文字位置全部可控,零额外对象。
//
//  注意这一列**没有把文本交给基类**。基类画完文字,我们再盖进度条,
//  文字就被埋了。所以进度条上那行 "42%" 也是自己 drawText 的。
// ===========================================================================
class ProgressDelegate : public CellBackgroundDelegate
{
    Q_OBJECT

public:
    explicit ProgressDelegate(QObject *parent = nullptr);

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &index) const override;

    //  高度固定,不需要逐行量。返回无效 QSize 表示"用默认行高"。
    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &index) const override;

    //  进度条按颜色区分:绿=已完成,蓝=进行中,红=失败,灰=排队。
    //  这个映射放在 delegate 里而不是模型里,因为"什么颜色"是表现层的事。
    static QColor colorFor(int progress, int status);
};
