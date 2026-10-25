#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "timelinewidget.h"

#include <QAudioOutput>
#include <QLabel>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QVideoWidget>

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openFile(const QString&path);

private slots:
    void openVideo();
    void togglePlay();

private:
    void buildUi();
    void connectSignals();
private:
    QMediaPlayer   *_player   = nullptr;
    QAudioOutput   *_audio    = nullptr;
    QVideoWidget   *_video    = nullptr;
    TimelineWidget *_timeline = nullptr;
    QAction        *_playAct  = nullptr;
    QLabel         *_timeLab  = nullptr;

};

#endif // MAINWINDOW_H
