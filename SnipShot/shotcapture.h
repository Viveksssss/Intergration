#pragma once

#include <QImage>
#include <QRect>


class QPainter;


namespace ShotCapture{


QRect virtualGeometry();
QImage grabAll();
QImage makeTestDesktop();

}
