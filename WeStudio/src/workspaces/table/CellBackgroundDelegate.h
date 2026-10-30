#pragma once

#include <QStyledItemDelegate>

class QPainter;
class QStyleOptionViewItem;
class QModelIndex;

// ===========================================================================
//  CellBackgroundDelegate —— 所有自写 delegate 的共同基类。
//
//  存在的唯一理由:**背景必须只定义一次**。
//
//  走自绘 delegate 之后,你会发现一个规律——每写一个新的 delegate,
//  开头那 10 行永远是同一段:
//
//      if (selected) fillRect(选中色);
//      else fillRect(row & 1 ? 奇色 : 偶色);
//      drawLine(右分隔线);
//
//  抄三份的后果不是"代码难看",是**改不动**:想给表格加个 hover 高亮,
//  要去三个文件里各改一遍,漏一个就出现"鼠标停在进度条上高亮、停在
//  说明文字上不高亮"的神奇现象。
//
//  所以抽一个 paintBackground(),三个 delegate 全都调它。
//  这不是过度设计——是"三处相同且都会变"时的标准做法。
//
//  另外还有一件基类该做的事:**没写自定义 delegate 的列**。
//  序号列、任务列、体积列用的是 Qt 默认 delegate,它不画斑马纹,
//  于是那几格会是纯底色,整行颜色断开(截图里一眼就能看到)。
//  解决办法是把默认 delegate 也换成这个基类的一个实例——它画完背景
//  就调基类去画文字,什么都不用自己实现。
// ===========================================================================
class CellBackgroundDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit CellBackgroundDelegate(QObject *parent = nullptr);

    //  三个 delegate 都调这个。做成 protected 是因为它不该对外——
    //  外部只关心"画这一格",不关心分几步。
    static void paintBackground(QPainter *p, const QStyleOptionViewItem &opt,
                                const QModelIndex &index);

protected:
    //  基类的 paint 默认实现:画背景 + 交给 QStyledItemDelegate 画文字。
    //  不需要自绘的列直接用它。
    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &index) const override;
};
