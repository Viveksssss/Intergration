#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPoint>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include "CardDrag.h"
#include "BoardTheme.h"

class CardWidget;
class BoardModel;

// ===========================================================================
//  ColumnWidget：一列。
//
//  职责边界（这块想清楚，整个工程就清了一半）：
//    · 负责**卡片子控件的几何**：谁在第几行、占位符占多高
//    · 负责**视觉**：列背景、列头、计数徽标、占位符
//    · 负责**接收拖放**：dragEnter / dragMove / drop
//    · 不负责数据：卡片增删改一律转给 BoardModel，做完再 refresh
//
//  为什么卡片位置要自己算？见 CardWidget.h 的注释。
// ===========================================================================
class ColumnWidget : public QWidget
{
    Q_OBJECT

public:
    ColumnWidget(int index, BoardModel *model, QWidget *parent = nullptr);

    int columnIndex() const { return m_index; }

    void rebuildCards();                  // 按模型重建卡片子控件
    void relayout(bool animate);          // 按当前状态重排位置

    // ---- 拖拽生命周期
    void beginDrag(int cardId);           // 记住被拖的卡，留出第一处占位
    void setDropTarget(const DropTarget &t);   // 落点变化
    void endDrag();

    CardWidget  *cardWidget(int id) const { return m_cards.value(id, nullptr); }
    QVector<int> cardOrder() const { return m_order; }

    // 插槽 i（0..n）在列内的 y。第 i 个插槽就是"第 i 张卡片的位置"，
    // 也是占位符可能出现的位置。
    int slotY(int i) const;
    int slotTop() const;

    // 装下所有卡片（含底部留白）所需的高度
    int contentHeight() const { return slotY(m_order.size()) + BoardTheme::kColumnPad; }

    QString debugSnapshot() const;

signals:
    void cardPressed(int cardId, const QPoint &grabOffset);
    void dragEntered(int col);
    void dragExited(int col);
    void hoverChanged(int col, const DropTarget &t, int yInColumn);
    void cardDropped(int cardId, int toCol, int toIndex);

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    void paintHeader(QPainter &p);
    void paintSlot(QPainter &p);

    DropTarget hitTest(const QPoint &pos) const;
    int  positionFromY(int y) const;
    int  draggedRowInThisColumn() const;

    int          m_index = 0;
    BoardModel  *m_model = nullptr;

    QVector<int>             m_order;     // 卡片 id，按显示顺序
    QHash<int, CardWidget *> m_cards;

    // ---- 拖放状态
    int        m_dragCard = -1;
    bool       m_hovered  = false;
    DropTarget m_target;

    // 占位符
    int     m_slotFrom = -1;              // 占位插槽号，-1 = 没有占位符
    qreal   m_slotDrawY = 0;              // 当前绘制位置（动画中间值）
    QTimer *m_slotAnim  = nullptr;
    QElapsedTimer m_slotClock;
    qreal   m_slotAnimFrom = 0;
    qreal   m_slotAnimTo   = 0;
    int     m_slotDurMs    = 0;
};
