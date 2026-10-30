#pragma once

#include "CellBackgroundDelegate.h"

// ===========================================================================
//  StatusDelegate —— 一个小圆点 + 状态文字。
//
//  单独写它不只是为了好看,而是为了演示一件事:
//  **delegate 是按列装的,不是按表装的。**
//
//  进度条列要 ProgressDelegate,说明列要 MultiLineDelegate,状态列要这个,
//  剩下几列用 Qt 自带的默认 delegate 就行。
//  用 setItemDelegateForColumn(列号, delegate) 逐列指定,
//  比"自己写一个全能的 delegate 然后 switch(index.column())"干净得多——
//  后者把所有绘制逻辑塞进一个文件,改一列要通读全部。
// ===========================================================================
class StatusDelegate : public CellBackgroundDelegate
{
    Q_OBJECT

public:
    explicit StatusDelegate(QObject *parent = nullptr);

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &index) const override;

    //  状态 → 颜色。放在这里和 ProgressDelegate::colorFor 是两套映射:
    //  进度条要看"值",圆点只看"状态",两套语义本来就不同,不该强行合并。
    static QColor dotColor(int status);
};
