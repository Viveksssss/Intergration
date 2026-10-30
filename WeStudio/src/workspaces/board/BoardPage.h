#pragma once

#include <QWidget>

#include "BoardModel.h"

class BoardWidget;
class QScrollArea;

// ===========================================================================
//  BoardPage：外壳。
//
//  结构很简单，但有一个关键决定：
//    QScrollArea（只在垂直方向滚）+ BoardWidget（横向排所有列）
//  而不是"每列各自一个滚动区"。理由见 手册 里"为什么列不自己滚"一节：
//  整页一起滚，卡片在列之间拖动时才不会出现"两列滚动偏移不同"的错位。
// ===========================================================================
class BoardPage : public QWidget
{
    Q_OBJECT

public:
    explicit BoardPage(QWidget *parent = nullptr);

    BoardWidget *board() const { return m_board; }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void paintTopBar(QPainter &p);

    BoardModel  *m_model = nullptr;
    QScrollArea *m_scroll = nullptr;
    BoardWidget *m_board  = nullptr;
};
