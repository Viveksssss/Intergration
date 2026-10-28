#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>

#include "Annotation.h"

// ===========================================================================
//  配色与字体集中在这里。
//  截图工具的界面几乎全是自绘（遮罩、手柄、尺寸标签、工具栏），
//  用一个头文件收口"颜色从哪来"，比到处写 #2ea8ff 好维护。
// ===========================================================================
namespace Theme {

// ---- 遮罩与选区 ----
inline const QColor kMask        (  0,   0,   0, 118);   // 选区外的暗色遮罩
inline const QColor kSelBorder   ("#2ea8ff");            // 选区边框
inline const QColor kHandleFill  ("#ffffff");            // 手柄填充
inline const QColor kSizeTagBg   ("#2ea8ff");            // 尺寸标签底色
inline const QColor kCross       (255, 255, 255,  70);   // 十字辅助线

// ---- 工具栏 ----
inline const QColor kPanelBg     ( 32,  32,  40, 250);
inline const QColor kPanelBorder ( 70,  70,  86);
inline const QColor kText        ("#e8e8f0");
inline const QColor kTextDim     ("#9a9aa8");
inline const QColor kHover       ("#35353f");
inline const QColor kActive      ("#2ea8ff");

// ---- 标注笔色 ----
inline const QColor kInkRed      ("#ff3b30");
inline const QColor kInkYellow   ("#ffcc00");
inline const QColor kInkGreen    ("#34c759");
inline const QColor kInkBlue     ("#0a84ff");
inline const QColor kInkWhite    ("#ffffff");

QFont uiFont(int pt, bool bold = false);
QFont monoFont(int pt);

// 程序与系统托盘图标
QIcon appIcon(int size = 32);
// 工具图标：全部用 QPainter 现画，不依赖任何图片资源
QIcon toolIcon(Tool t, int size = 18);
// 动作图标："undo" / "trash" / "copy" / "save" / "close"
QIcon actionIcon(const char *name, int size = 18);

}  // namespace Theme
