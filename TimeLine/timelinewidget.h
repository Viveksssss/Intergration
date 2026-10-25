#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include <QObject>
#include <QVariantAnimation>
#include <QWidget>

class TimelineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget *parent = nullptr);
    void setDuration(qint64 ms);
    void setPosition(qint64 ms);
    void resetView();
    void setTitle(const QString&t);

    bool isLoaded()const{return _duration > 0;}

protected:
    virtual void paintEvent(QPaintEvent*)override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void wheelEvent(QWheelEvent*event)override;
    virtual void keyPressEvent(QKeyEvent *event) override;

signals:
    void seekRequested(qint64 ms);  // 跳转
    void togglePlayRequested();     // 空格键
private:
    /*xx:xx to X*/
    double timeToX(double ms)const;
    /*X to xx:xx*/
    double xToTime(double x)const;

    /*绘制轨道+刻度+框选+播放头*/
    void drawTrack(QPainter&p);
    void drawRuler(QPainter&p);
    void drawSelection(QPainter&p);
    void drawPlayHead(QPainter&p);
    void drawFooter(QPainter &p);

    /*切换视口*/
    void zoomToRange(double a,double b);
    void animateViewTo(double a,double b);
    void setView(double a,double b);

    double pickStep()const;
private:
    /*可见窗口七点*/
    double _viewStart = 0;
    /*可见窗口重点*/
    double _viewEnd = 1;
    /*总时长ms*/
    qint64 _duration = 0;
    /*播放头位置*/
    qint64 _position = 0;
    QString _title;

    /*框选*/
    bool _dragging = false;
    QPoint _pressPos,_curPos;

    /*动画*/
    QVariantAnimation _animation;
    double _fromS = 0,_fromE = 1;
    double _toS = 0,_toE = 1;

    // QWidget interface
protected:

};

#endif // TIMELINEWIDGET_H
