#pragma once

#include "CellBackgroundDelegate.h"

// ===========================================================================
//  MultiLineDelegate —— 在格子里换行画一段长文本,超出就省略。
//
//  三件事必须一起做对,少一件就出问题:
//
//  1. **换行**:QPainter::drawText(rect, flags, text) 带 Qt::TextWordWrap
//     就会按词换行。中文没有空格,Qt 会按字断,效果是对的。
//
//  2. **截断**:超出矩形范围怎么办。Qt 的 TextWordWrap 会自动裁到 rect,
//     但**不会告诉你"被裁了"**,也不会加省略号。想加省略号必须自己算:
//     用 QTextLayout 逐行 layout,排到放不下为止,最后一行用 elidedText。
//
//  3. **行高**:换行以后这一行要多高?这必须由 sizeHint 提前给出来。
//     如果 sizeHint 不给,视图用默认行高,文字下半截被切掉——
//     这是多行 delegate 最常见的 bug,而且看起来像"字体排版错了",
//     很容易往错误方向查。
//
//  本文件用 QTextLayout 而不是 drawText+WordWrap,就是为了第 2 点:
//  drawText 给你"换行",QTextLayout 给你"换行 + 我知道放不下"。
// ===========================================================================
class MultiLineDelegate : public CellBackgroundDelegate
{
    Q_OBJECT

public:
    explicit MultiLineDelegate(int maxLines = 2, QObject *parent = nullptr);

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &opt,
                   const QModelIndex &index) const override;

    //  自检要能验证"两行文本真的排成了两行"。
    //  单独暴露一个纯函数,不依赖视图状态——这样自检不必真的截图。
    //  返回实际排出的行数(含那些因为放不下而没画的行由 maxLines 卡住)。
    int layoutLines(const QFont &font, const QString &text, int width,
                    QStringList *outLines = nullptr) const;

    int maxLines() const { return m_maxLines; }
    void setMaxLines(int n) { m_maxLines = qMax(1, n); }

    //  自检用:真正绘制时对某一格排出了几行。
    //  paint() 是 const 且要 QPainter,不方便在自检里直接调,
    //  所以把"算行数"的逻辑抽成纯函数,自检喂同样的入参就能验证。
    struct LayoutResult {
        int  lines     = 0;     // 实际排出的行数
        bool truncated = false; // 是否还有内容没排下
    };
    LayoutResult measure(const QFont &font, const QString &text,
                         const QSize &cellSize) const;

private:
    int m_maxLines;
};
