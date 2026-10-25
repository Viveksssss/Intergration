#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QString>
#include <qapplication.h>


namespace Theme{

// ── 画布
inline const QColor kCanvasBg       (0x1b,0x1b,0x21);
inline const QColor kGridFine       (255, 255, 255, 10);
inline const QColor kGridCoarse     (255, 255, 255, 26);

// ── 节点
inline const QColor kNodeBg         (0x2b,0x2b,0x35);
inline const QColor kNodeBgSelected (0x32,0x32,0x3f);
inline const QColor kNodeBorder     (0x3f,0x3f,0x4d);
inline const QColor kNodeBorderHot  (0x4d,0xa3,0xff);
inline const QColor kTitleBg        (0x3a,0x3a,0x47);
inline const QColor kTitleText      (0xec,0xec,0xf5);
inline const QColor kBodyText       (0x8f,0x8f,0x9d);
inline const QColor kValueText      (0xe8,0xe8,0xf2);
inline const QColor kShadow         (0, 0, 0, 70);

// ── 端口：按数据类型上色，一眼看出能不能连
inline const QColor kPortNumber     (0xff,0xcf,0x5c);   // 数值
inline const QColor kPortAny        (0xb4,0x8e,0xf0);   // 任意
inline const QColor kPortRing       (0x1b,0x1b,0x21);   // 端口描边（和画布同色，像挖了个洞）

// ── 连线
inline const QColor kEdge           (0x76,0x76,0x88);
inline const QColor kEdgeHot        (0xFF,0xE4,0xE1);   // 选中 / 悬停
inline const QColor kEdgeTemp       (0x5a,0xc8,0xfa);   // 拖拽中的临时线

// 应用调色板 + 样式表
void apply(QApplication &app);

}


#endif // THEME_H
