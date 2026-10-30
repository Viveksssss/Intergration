#include "model.h"

#include <QDataStream>
#include <QIODevice>
#include <QUuid>

#include <algorithm>

namespace {
const bool registered = [] {
    qRegisterMetaType<CanvasStroke>("CanvasStroke");
    qRegisterMetaType<BoardEvent>("BoardEvent");
    return true;
}();

QDataStream &configure(QDataStream &stream)
{
    stream.setVersion(QDataStream::Qt_6_2);
    return stream;
}
}

bool BoardEvent::isValid() const
{
    if (id.isEmpty()) {
        return false;
    }
    switch (kind) {
    case EventKind::Stroke:
        return !points.isEmpty() && width > 0;
    case EventKind::Clear:
        return true;
    case EventKind::Chat:
        return !text.trimmed().isEmpty();
    }
    return false;
}

QByteArray BoardEvent::toPayload() const
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    configure(stream);
    stream << seq
           << quint8(kind)
           << id
           << author
           << text
           << color.name(QColor::HexArgb)
           << qint32(width)
           << points;
    return payload;
}

bool BoardEvent::fromPayload(const QByteArray &payload, BoardEvent *event)
{
    if (!event) {
        return false;
    }
    QDataStream stream(payload);
    configure(stream);
    quint8 kindValue = 0;
    QString colorName;
    qint32 widthValue = 0;
    BoardEvent parsed;
    stream >> parsed.seq
           >> kindValue
           >> parsed.id
           >> parsed.author
           >> parsed.text
           >> colorName
           >> widthValue
           >> parsed.points;
    if (stream.status() != QDataStream::Ok) {
        return false;
    }
    parsed.kind = static_cast<EventKind>(kindValue);
    parsed.color = QColor(colorName);
    parsed.width = widthValue;
    if (!parsed.isValid()) {
        return false;
    }
    *event = parsed;
    return true;
}

void BoardModelState::clear()
{
    strokes.clear();
    chats.clear();
}

void BoardModelState::apply(const BoardEvent &event)
{
    if (event.kind == EventKind::Stroke) {
        const auto found = std::find_if(
            strokes.begin(), strokes.end(),
            [&event](const BoardEvent &item) { return item.id == event.id; });
        if (found == strokes.end()) {
            strokes.append(event);
        } else {
            *found = event;
        }
    } else if (event.kind == EventKind::Clear) {
        strokes.clear();
    } else if (event.kind == EventKind::Chat) {
        chats.append(event);
        constexpr qsizetype kChatSnapshotLimit = 200;
        if (chats.size() > kChatSnapshotLimit) {
            chats.remove(0, chats.size() - kChatSnapshotLimit);
        }
    }
}

QVector<BoardEvent> BoardModelState::snapshot() const
{
    QVector<BoardEvent> events = strokes + chats;
    std::sort(events.begin(), events.end(),
              [](const BoardEvent &left, const BoardEvent &right) {
                  return left.seq < right.seq;
              });
    return events;
}
