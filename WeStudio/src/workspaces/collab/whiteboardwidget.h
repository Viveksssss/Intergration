#pragma once

#include "model.h"

#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QPainter;

class WhiteboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WhiteboardWidget(QWidget *parent = nullptr);

    int strokeCount() const;
    void applyStroke(const CanvasStroke &stroke, bool repaint = true);
    void clearCanvas();
    void refresh();

signals:
    void strokeCommitted(const CanvasStroke &stroke);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void drawStroke(QPainter &painter, const CanvasStroke &stroke) const;

    QVector<CanvasStroke> m_strokes;
    CanvasStroke m_activeStroke;
    bool m_drawing = false;
};
