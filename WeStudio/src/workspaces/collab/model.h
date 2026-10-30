#pragma once

#include <QByteArray>
#include <QColor>
#include <QMetaType>
#include <QPointF>
#include <QString>
#include <QVector>

enum class EventKind : quint8
{
    Stroke = 1,
    Clear = 2,
    Chat = 3
};

struct CanvasStroke
{
    QString id;
    QString author;
    QColor color = QColor("#2563eb");
    int width = 3;
    QVector<QPointF> points;

    bool isEmpty() const
    {
        return id.isEmpty() || points.isEmpty();
    }
};

struct BoardEvent
{
    EventKind kind = EventKind::Stroke;
    quint64 seq = 0;
    QString id;
    QString author;
    QString text;
    QColor color = QColor("#2563eb");
    int width = 3;
    QVector<QPointF> points;

    bool isValid() const;
    QByteArray toPayload() const;
    static bool fromPayload(const QByteArray &payload, BoardEvent *event);
};

struct BoardModelState
{
    QVector<BoardEvent> strokes;
    QVector<BoardEvent> chats;

    void clear();
    void apply(const BoardEvent &event);
    QVector<BoardEvent> snapshot() const;
};

Q_DECLARE_METATYPE(CanvasStroke)
Q_DECLARE_METATYPE(BoardEvent)
