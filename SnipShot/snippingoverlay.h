#ifndef SNIPPINGOVERLAY_H
#define SNIPPINGOVERLAY_H

#include "annotation.h"
#include <QWidget>
#include <QImage>

class ToolBar;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QLineEdit;
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

    QImage result()const;
    void selectTool(Tool t);        // 也供快捷键调用
signals:
    void cancelled();
    void finished(const QImage&image);
    
protected:
    void paintEvent(QPaintEvent*e)override;
    void mouseMoveEvent(QMouseEvent*e)override;
    void keyPressEvent(QKeyEvent*e)override;
    void showEvent(QShowEvent*e)override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent*e)override;
    void resizeEvent(QResizeEvent*e)override;

private:
    enum class Mode { Idle, Framing ,Moving,Resizing,Drawing};

    void layoutToolBar();
    void finishAndCopy();
    void saveToFile();

    // 绘制
    void paintSelectionChrome(QPainter &p);
    void paintCrosshair(QPainter &p);

    // 命中与交互
    Handle handleAt(const QPoint&pos )const;
    QRect handleRect(Handle h) const;
    void applyCursorShape(const QPoint &pos);
    void moveSelectionBy(const QPointF&d);
    void updateDraft(const QPoint &pos);

    // 标注管理
    void pushUndo();
    void popUndo();
    void undo();
    void redo();
    void clearAnnotations();
    void beginTextInput(const QPoint&pos);
    void commitText();

    // 截图
    QImage _shot;

    // 选取
    QRect  _sel;
    bool   _hasSel = false;

    // 抓手
    Mode   _mode = Mode::Idle;
    Handle _handle = Handle::None;

    // 工具
    Tool _tool = Tool::None;
    QColor _color = QColor(0xff, 0x3b, 0x30);
    int _penWidth =3;
    int _fontSize = 22;
    int _block = 10;

    QVector<Annotation>_anns;
    Annotation _draft;
    bool _hasDraft = false;
    QVector<QPointF>_penPoints;
    QVector<QVector<Annotation>> _undo;
    QVector<QVector<Annotation>> _redo;

    // 坐标
    QPoint _mousePos;
    QPoint _lastPos;
    QPoint _pressPos;
    QRect _pressSel;

    ToolBar *_bar =nullptr;
    QLineEdit*_textEdit = nullptr;
    QPoint _textAnchor;

};

#endif // SNIPPINGOVERLAY_H
