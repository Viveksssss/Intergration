#pragma once

#include <QGraphicsView>

class NodeScene;

// ---------------------------------------------------------------------------
//  NodeView —— 那个"无限画布"。
//
//  QGraphicsView 天生就是个带变换的视口，所以"无限画布"不用自己算，
//  只要做三件事：
//    ① setSceneRect 给一个巨大的矩形，让滚动条有得滚
//    ② 滚轮缩放时把锚点钉在鼠标位置（AnchorUnderMouse）
//    ③ 自己画网格背景，并且网格密度随缩放自动换挡
// ---------------------------------------------------------------------------
class NodeView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit NodeView(NodeScene *scene, QWidget *parent = nullptr);

    double zoom() const { return m_zoom; }

public slots:
    void resetZoom();

signals:
    void zoomChanged(double zoom);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    double m_zoom = 1.0;
    bool   m_panning = false;
    QPoint m_panStart;
};
