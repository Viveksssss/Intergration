#include "whiteboardwidget.h"

#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUuid>

#include <algorithm>

WhiteboardWidget::WhiteboardWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(640, 420);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

int WhiteboardWidget::strokeCount() const
{
    return m_strokes.size() + (m_drawing ? 1 : 0);
}

void WhiteboardWidget::applyStroke(const CanvasStroke &stroke, bool repaint)
{
    if (stroke.isEmpty()) {
        return;
    }
    const auto duplicate = std::find_if(
        m_strokes.cbegin(), m_strokes.cend(),
        [&stroke](const CanvasStroke &item) { return item.id == stroke.id; });
    if (duplicate == m_strokes.cend()) {
        m_strokes.append(stroke);
        if (repaint) {
            update();
        }
    }
}

void WhiteboardWidget::clearCanvas()
{
    m_strokes.clear();
    m_activeStroke = {};
    m_drawing = false;
    update();
}

void WhiteboardWidget::refresh()
{
    update();
}

void WhiteboardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_drawing = true;
    m_activeStroke = {};
    m_activeStroke.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_activeStroke.author = tr("本地");
    m_activeStroke.points.append(event->position());
    update();
}

void WhiteboardWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_drawing) {
        return;
    }
    const QPointF point = event->position();
    if (m_activeStroke.points.isEmpty()
        || QLineF(m_activeStroke.points.constLast(), point).length() >= 1.5) {
        m_activeStroke.points.append(point);
        update();
    }
}

void WhiteboardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_drawing || event->button() != Qt::LeftButton) {
        return;
    }
    if (m_activeStroke.points.isEmpty()) {
        m_activeStroke.points.append(event->position());
    }
    m_drawing = false;
    m_strokes.append(m_activeStroke);
    emit strokeCommitted(m_activeStroke);
    m_activeStroke = {};
    update();
}

void WhiteboardWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);

    painter.setPen(QPen(QColor("#edf1f7"), 1));
    constexpr int gridStep = 32;
    for (int x = 0; x < width(); x += gridStep) {
        painter.drawLine(x, 0, x, height());
    }
    for (int y = 0; y < height(); y += gridStep) {
        painter.drawLine(0, y, width(), y);
    }

    for (const CanvasStroke &stroke : m_strokes) {
        drawStroke(painter, stroke);
    }
    if (m_drawing) {
        drawStroke(painter, m_activeStroke);
    }
}

void WhiteboardWidget::drawStroke(QPainter &painter, const CanvasStroke &stroke) const
{
    if (stroke.points.isEmpty()) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(stroke.color, stroke.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);

    if (stroke.points.size() == 1) {
        painter.setBrush(stroke.color);
        painter.drawEllipse(stroke.points.constFirst(), stroke.width / 2.0,
                            stroke.width / 2.0);
        return;
    }

    QPainterPath path(stroke.points.constFirst());
    for (qsizetype i = 1; i < stroke.points.size(); ++i) {
        path.lineTo(stroke.points.at(i));
    }
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}
