#ifndef SNIPPINGOVERLAY_H
#define SNIPPINGOVERLAY_H

#include <QWidget>
#include <QImage>
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class SnippingOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit SnippingOverlay(QWidget *parent = nullptr);

signals:
    void cancelled();
    
protected:
    void paintEvent(QPaintEvent*e)override;
    void mouseMoveEvent(QMouseEvent*e)override;
    void keyPressEvent(QKeyEvent*e)override;
    void showEvent(QShowEvent*e)override;
};

#endif // SNIPPINGOVERLAY_H
