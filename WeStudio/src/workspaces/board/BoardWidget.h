#pragma once

#include <QPoint>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include "CardDrag.h"
#include "BoardTheme.h"       // edgeScrollTrigger() 内联用到 kEdgeScrollY

class CardWidget;
class ColumnWidget;
class BoardModel;
class QScrollArea;

// ===========================================================================
//  BoardWidget：横向排列的一组列。
//
//  它承担三个"跨列"职责，单列做不到：
//    1. 发起 QDrag（卡片被按下并开始移动时）
//    2. 把各列报上来的落点统一广播给所有列 —— 保证全局只有一个占位符
//    3. 边缘自动滚动（拖到上下边缘时定时滚）
//
//  它**不**直接处理 wheel / drop：wheel 落在列上、drop 也落在列上，
//  两者都通过信号上抛到这里，因为这个类才知道 QScrollArea 的存在。
// ===========================================================================
class BoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BoardWidget(BoardModel *model, QWidget *parent = nullptr);

    ColumnWidget *column(int i) const { return m_cols.value(i, nullptr); }
    int columnCount() const { return m_cols.size(); }

    void rebuild();                       // 数据变了，重建各列的子控件
    void relayoutAll(bool animate);       // 只重排位置
    void refreshHeights();                // 内容高度变了，重新算列高与滚动范围

    // ---- 自检钩子：绕过真实的 QDrag 直接驱动各列
    QString debugSnapshot() const;
    void applyHover(int col, const DropTarget &t);
    // 自检用：把 board 切进/切出拖拽态（真实运行时由 startDrag/clearDragState 做）
    void beginDragProbe(int cardId);
    // 真实运行时 clearDragState() 是在 drag->exec() 返回后调的（阻塞式），
    // 自检里没有 exec()，就只能手工补这一步，否则会一直停在拖拽态。
    // 包一层而不直接把 clearDragState 公开，是为了让"它只该在收尾时调"这件事
    // 在头文件里就有约束感。
    void endDragProbe() { clearDragState(); }
    void applyDrop(int cardId, int toCol, int toIndex);

    // 边缘自动滚动的探测入口。真实运行时由 ColumnWidget::dragMoveEvent
    // 上抛的 hoverChanged 顺带调用；自检时直接调它。
    //   yInColumn = 鼠标在列控件里的 y；传 -1 表示"离开边缘/离开列"。
    void applyEdgeProbe(int yInColumn);

    int  scrollStep() const { return m_scrollStep; }
    int  scrollValue() const;

    // 边缘感应区高度：进入列顶部 / 底部这么多像素内就开滚
    static int edgeScrollTrigger() { return BoardTheme::kEdgeScrollY; }

    bool isDragging() const { return m_dragging; }
    int  dragCard() const { return m_dragCard; }
    int  hoverColumn() const { return m_hoverCol; }

signals:
    void modelChanged();

protected:
    void resizeEvent(QResizeEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    void startDrag(int cardId, const QPoint &grabOffset);
    void clearDragState();
    void onHoverChanged(int col, const DropTarget &t);

    QScrollArea *scrollArea() const;

    BoardModel              *m_model = nullptr;
    QVector<ColumnWidget *>  m_cols;

    bool m_dragging = false;
    int  m_dragCard = -1;
    int  m_hoverCol = -1;

    // 边缘自动滚动
    QTimer *m_scrollTimer = nullptr;
    int     m_scrollStep  = 0;      // 每次 tick 的像素数，0 = 不滚
};
