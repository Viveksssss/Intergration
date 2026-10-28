#pragma once

#include <QColor>
#include <QLineF>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>

// ===========================================================================
//  一个「标注」就是一个能被画出来的东西。
//  所有几何量都用**全局逻辑坐标**（= 屏幕坐标，也是 overlay 窗口的坐标），
//  这样它既能直接画在遮罩窗口上，也能在导出时被平移后重画一次。
// ===========================================================================

enum class Tool {
    None,      // 不画东西，鼠标用来拖动 / 调整选区
    Rect,      // 矩形框
    Arrow,     // 箭头
    Pen,       // 自由画笔
    Text,      // 文字
    Mosaic,     // 马赛克
    Cursor      // 光标
};

struct Annotation
{
    Tool         tool  = Tool::Rect;
    QColor       color = QColor(0xff, 0x3b, 0x30);
    int          width = 3;          // 线宽（逻辑像素）

    QRectF       rect;               // Rect / Mosaic 的外接矩形
    QPointF      p1, p2;             // Arrow：尾 → 头
    QPainterPath path;               // Pen：已经平滑过的路径
    QPointF      textPos;            // Text：文字左上角
    QString      text;
    int          fontSize = 22;
    int          block    = 12;      // Mosaic：每块多大（逻辑像素）

    bool isEmpty() const;
    void translateBy(const QPointF &d);
};

inline bool Annotation::isEmpty() const
{
    switch (tool) {
    case Tool::Rect:
    case Tool::Mosaic:
        return rect.width() < 2 || rect.height() < 2;
    case Tool::Arrow:
        return QLineF(p1, p2).length() < 3;
    case Tool::Pen:
        return path.isEmpty();
    case Tool::Text:
        return text.trimmed().isEmpty();
    case Tool::None:
        break;
    }
    return true;
}

// 拖动选区时，选区里的标注必须整体平移。
// 每种工具要改的字段不一样，所以统一收在这里，
// 以后加新工具只改这一个函数。
inline void Annotation::translateBy(const QPointF &d)
{
    switch (tool) {
    case Tool::Cursor:{
        break;
    }
    case Tool::Rect:
    case Tool::Mosaic:
        rect.translate(d);
        break;
    case Tool::Arrow:
        p1 += d;
        p2 += d;
        break;
    case Tool::Pen:
        path.translate(d.x(), d.y());
        break;
    case Tool::Text:
        textPos += d;
        break;
    case Tool::None:
        break;
    }
}
