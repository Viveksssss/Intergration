#pragma once

#include <QImage>
#include <QRect>
#include <QVector>
#include <QWidget>

#include "Annotation.h"

class QKeyEvent;
class QLineEdit;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class ToolBar;

// 选区的 8 个控制点 + 整体拖动。
// 放这里而不是 Annotation.h：它是"怎么调选区"的概念，和"画什么"无关。
enum class Handle {
    None, Move,
    Left, Top, Right, Bottom,
    TopLeft, TopRight, BottomLeft, BottomRight
};

// ===========================================================================
//  全屏截图遮罩窗口。
//
//  它同时扮演三个角色：
//    1. 展示"定格的屏幕"（手里那张 QImage），并压一层半透明遮罩
//    2. 接收鼠标，管理选区、手柄、各种标注草稿
//    3. 承载浮动工具栏，最后把结果渲染出来
//
//  为什么用一个全屏窗口而不是几个分开的控件？
//    因为遮罩必须连成一片，选区又要"挖洞"——只有一个窗口才好算。
// ===========================================================================
class SnippingOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit SnippingOverlay(const QImage &shot, QWidget *parent = nullptr);
    ~SnippingOverlay() override;

    // 自检用：跳过鼠标，直接塞一个选区和几笔标注进来
    void setDemoScene(const QRect &sel, const QVector<Annotation> &anns);

    QImage result() const;          // 按当前选区 + 标注渲染出的最终图

    void selectTool(Tool t);        // 也供快捷键调用

    // 自检用：把内部状态拼成一行文本，方便自动化测试断言
    QString debugSnapshot() const;

signals:
    void finished(const QImage &image);   // 复制 / 保存后都会发这个
    void cancelled();

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;

private:
    enum class Mode { Idle, Framing, Moving, Resizing, Drawing };

    // ---- 绘制
    void paintSelectionChrome(QPainter &p);
    void paintCrosshair(QPainter &p);

    // ---- 命中与交互
    Handle handleAt(const QPoint &pos) const;
    QRect  handleRect(Handle h) const;
    void   applyCursorShape(const QPoint &pos);
    void   updateDraft(const QPoint &pos);
    void   moveSelectionBy(const QPoint &d);

    // ---- 标注管理
    void pushUndo();
    void popUndo();
    void undo();
    void redo();
    void clearAnnotations();
    void beginTextInput(const QPoint &pos);
    void commitText();

    // ---- 收尾
    void layoutToolBar();
    void finishAndCopy();
    void saveToFile();

    // ---- 数据
    QImage m_shot;

    QRect  m_sel;
    bool   m_hasSel = false;

    Mode   m_mode   = Mode::Idle;
    Handle m_handle = Handle::None;

    QPoint m_pressPos;
    QPoint m_lastPos;
    QPoint m_mousePos;
    QRect  m_pressSel;          // 按下瞬间的选区快照（move/resize 的基准）

    Tool   m_tool     = Tool::None;
    QColor m_color    = QColor(0xff, 0x3b, 0x30);
    int    m_penWidth = 3;
    int    m_block    = 12;
    int    m_fontSize = 22;

    QVector<Annotation> m_anns;
    Annotation          m_draft;
    bool                m_hasDraft = false;
    QVector<QPointF>    m_penPoints;      // 画笔的原始采样点

    QVector<QVector<Annotation>> m_undo;
    QVector<QVector<Annotation>> m_redo;

    ToolBar   *m_bar      = nullptr;
    QLineEdit *m_textEdit = nullptr;
    QPoint     m_textAnchor;
};
