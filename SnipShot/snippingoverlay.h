#ifndef SNIPPINGOVERLAY_H
#define SNIPPINGOVERLAY_H

#include <QWidget>
#include <QImage>
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

enum class Handle {
    None, Move,
    Left, Top, Right, Bottom,
    TopLeft, TopRight, BottomLeft, BottomRight
};

class SnippingOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit SnippingOverlay(const QImage &shot,QWidget *parent = nullptr);
    ~SnippingOverlay();

    void setDemoScene(const QRect &sel);
    QString debugSnapshot() const;
signals:
    void cancelled();
    
protected:
    void paintEvent(QPaintEvent*e)override;
    void mouseMoveEvent(QMouseEvent*e)override;
    void keyPressEvent(QKeyEvent*e)override;
    void showEvent(QShowEvent*e)override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    enum class Mode { Idle, Framing ,Moving,Resizing};

    void paintSelectionChrome(QPainter &p);
    void paintCrosshair(QPainter &p);

    Handle handleAt(const QPoint&pos )const;
    QRect handleRect(Handle h) const;
    void applyCursorShape(const QPoint &pos);
    void moveSelectionBy(const QPoint&d);

    QImage _shot;

    QRect  _sel;
    bool   _hasSel = false;

    Mode   _mode = Mode::Idle;
    Handle _handle = Handle::None;

    QPoint _mousePos;
    QPoint _lastPos;
    QPoint _pressPos;
    QPoint _pressSel;

};

#endif // SNIPPINGOVERLAY_H
