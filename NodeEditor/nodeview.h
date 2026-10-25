#ifndef NODEVIEW_H
#define NODEVIEW_H

#include <QGraphicsView>

class NodeItem;
/*
无限画布
*/

class QGraphicsScene;
class NodeView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit NodeView(QGraphicsScene*scene,QWidget*parent = nullptr);
    double zoom()const{return _zoom;}

protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void drawBackground(QPainter *painter, const QRectF &rect) override;

public slots:
    void resetZoom();
signals:
    void zoomChanged(double zoom);

private:
    /*是否正在拖动*/
    bool _panning = false;
    /*拖动的起点*/
    QPoint _panStart;
    QPoint _position;
    QPointF _selectedPos;
    bool _hasSelection;
    NodeItem*_selectedNode = nullptr;
    double _zoom = 1.0;


    // QWidget interface
protected:
    virtual void keyPressEvent(QKeyEvent *event) override;

    // QWidget interface
protected:
    virtual void wheelEvent(QWheelEvent *event) override;
};

#endif // NODEVIEW_H
