#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QSet>
#include <QTimer>
#include <QWidget>

class CardWidget;

// ===========================================================================
//  CardWidget：一张卡片。
//
//  **它不参与布局**。位置完全由列控件的 relayout() 用 setGeometry 直接写。
//  如果用 QVBoxLayout，布局会在下一次 QEvent::LayoutRequest 时
//  把动画中的位置全部覆盖回布局算出来的值——动画就"看不见"了。
// ===========================================================================
class CardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CardWidget(int id, QWidget *parent = nullptr);

    int cardId() const { return m_id; }
    void setData(const QString &title, const QString &tag,
                 const QColor &tagColor, const QString &footer);

    QPoint  targetPos() const { return m_target; }
    QPointF drawPos() const { return m_draw; }

    // 动画到 pos；animate=false 时立即到位
    void moveTo(const QPoint &pos, bool animate);
    void jumpTo(const QPoint &pos);

    bool isAnimating() const { return m_anim != nullptr; }
    void finishAnimation();

    // 拖拽期间卡片本体被隐藏（屏幕上的"那张卡"其实是 QDrag 的幽灵图），
    // 但它的**占位**还在，所以内部动画不能停。
    void setGrabbed(bool on);

    // 拖拽时给幽灵图用的数据
    QString titleText() const { return m_title; }
    QString tagText() const { return m_tag; }
    QColor  tagColor() const { return m_tagColor; }
    QString footerText() const { return m_footer; }

    // 离屏渲染一张卡片，给 QDrag 当幽灵图
    static QPixmap renderPixmap(const QString &title, const QString &tag,
                                const QColor &tagColor, const QString &footer,
                                int width, qreal dpr);

signals:
    void pressed(int cardId, const QPoint &grabOffset);
    void released(int cardId);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;

private:
    int      m_id = 0;
    QString  m_title, m_tag, m_footer;
    QColor   m_tagColor;
    QRect    m_tagRect;

    QPoint   m_target;      // 目标位置（列内局部坐标）
    QPointF  m_draw;        // 当前显示位置

    QPoint   m_pressGlobal; // 按下点（全局坐标，拖拽判定用）
    bool     m_mouseDown = false;
    bool     m_grabbed   = false;
    bool     m_hover     = false;

    void stepAnimation();

    QTimer       *m_anim = nullptr;
    QElapsedTimer m_clock;
    QPointF       m_animFrom;
    QPoint        m_animTo;
    int           m_durMs = 0;
};
