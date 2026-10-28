#pragma once

#include "Annotation.h"

#include <QImage>
#include <QRect>


class QPainter;


namespace ShotCapture{


QRect virtualGeometry();
QImage grabAll();
QImage makeTestDesktop();

void paintOne(QPainter&p,const QImage&shot,const Annotation&a);
QImage renderRegion(const QImage&shot,const QRect&sel,const QVector<Annotation>&anns);
}
