#include "mainwindow.h"

#include <QApplication>
#include <QLabel>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include "shotcapture.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    auto p = ShotCapture::makeTestDesktop();
    MainWindow w;
    auto *l = new  QLabel();
    auto pp = QPixmap::fromImage(p);
    l->setPixmap(pp);
    w.setCentralWidget(l);
    w.show();

    return a.exec();
}
