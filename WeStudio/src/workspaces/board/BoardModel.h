#pragma once

#include <QColor>
#include <QString>
#include <QVector>

// ===========================================================================
//  纯数据：不碰任何 Qt Widgets。
//
//  这样做的好处是自检能在不创建界面的情况下直接构造、直接改，
//  而且从"卡片在哪个位置"到"界面上该长什么样"只有一条单向的路径：
//  数据模型改 → 视图重排。
// ===========================================================================
struct Card
{
    int     id = 0;          // 全局唯一，拖放时靠它认卡
    QString title;
    QString tag;             // 标签文字（"UI" / "Bug" …）
    QColor  tagColor;
    QString footer;          // 右下角的小字（"#12  3 天前"）
};

struct Column
{
    QString     title;
    QColor      accent;      // 列头小圆点 / 计数徽标颜色
    QVector<Card> cards;
};

// 看板的整个状态。移动卡片就是在这上面做一次 removeAt + insert。
class BoardModel
{
public:
    static BoardModel demo();          // 一个演示用的初始看板

    const QVector<Column> &columns() const { return m_cols; }
    QVector<Column>       &columns()       { return m_cols; }

    // 找卡片所在的列与行；找不到返回 false
    bool locate(int cardId, int *col, int *row) const;
    const Card *card(int cardId) const;

    // 把卡片从原位置移到 (toCol, toIndex)。toIndex 是**目标列里**的插入位置。
    // 返回 true 表示真的动了（原地拖放返回 false，调用方据此决定要不要入撤销栈）。
    bool moveCard(int cardId, int toCol, int toIndex);

    // 在 (col, row) 之后插入一张新卡片（self test / 演示用）
    void insertCard(int col, int row, const Card &c);

private:
    QVector<Column> m_cols;
};
