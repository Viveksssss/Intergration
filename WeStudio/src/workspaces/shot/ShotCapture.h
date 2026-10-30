#pragma once

#include <QImage>
#include <QRect>
#include <QVector>

#include "Annotation.h"

class QPainter;

// ===========================================================================
//  抓屏 + 渲染，全部与界面无关的纯函数。
//
//  坐标约定（整个工程只有这一套）：
//    · 逻辑坐标  —— 屏幕坐标，也是 overlay 窗口坐标、鼠标事件坐标
//    · 设备像素  —— 图片里真实的像素，= 逻辑坐标 × devicePixelRatio
//  只有 QImage::copy() / QImage::scaled() 这类像素级 API 需要设备像素，
//  其余一律用逻辑坐标，换算集中在 drawMosaic() 一个地方。
// ===========================================================================
namespace ShotCapture {

QRect  virtualGeometry();          // 所有屏幕的外接矩形（逻辑坐标）
QImage grabAll();                  // 抓取整个虚拟桌面，返回带 dpr 的图

// 画一张假的"桌面"：自检截图用。
// 好处是结果可复现，也不会把使用者真实桌面上的内容写进示意图。
QImage makeTestDesktop();

// 在任意 painter 上画一个标注（overlay 和导出共用同一份代码）
void   paintOne(QPainter &p, const QImage &shot, const Annotation &a);

// 按选区裁剪，并把标注重画上去 —— 这就是"导出"的全部
QImage renderRegion(const QImage &shot, const QRect &sel,
                    const QVector<Annotation> &anns);

}  // namespace ShotCapture
