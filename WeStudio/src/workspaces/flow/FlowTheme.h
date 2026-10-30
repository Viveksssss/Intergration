#pragma once

#include <QColor>
#include <QString>

class QApplication;

// ---------------------------------------------------------------------------
//  所有颜色 / 字体 / 样式表集中在这一个文件。
//  想换配色只改这里，不用去翻绘制代码。
// ---------------------------------------------------------------------------
namespace FlowTheme {

// ── 画布
inline const QColor kCanvasBg       ("#1b1b21");
inline const QColor kGridFine       (255, 255, 255, 10);
inline const QColor kGridCoarse     (255, 255, 255, 26);

// ── 节点
inline const QColor kNodeBg         ("#2b2b35");
inline const QColor kNodeBgSelected ("#32323f");
inline const QColor kNodeBorder     ("#3f3f4d");
inline const QColor kNodeBorderHot  ("#4da3ff");
inline const QColor kTitleBg        ("#3a3a47");
inline const QColor kTitleText      ("#ececf5");
inline const QColor kBodyText       ("#8f8f9d");
inline const QColor kValueText      ("#e8e8f2");
inline const QColor kShadow         (0, 0, 0, 70);

// ── 端口：按数据类型上色，一眼看出能不能连
inline const QColor kPortNumber     ("#ffcf5c");   // 数值
inline const QColor kPortAny        ("#b48ef0");   // 任意
inline const QColor kPortRing       ("#1b1b21");   // 端口描边（和画布同色，像挖了个洞）

// ── 连线
inline const QColor kEdge           ("#767688");
inline const QColor kEdgeHot        ("#4da3ff");   // 选中 / 悬停
inline const QColor kEdgeTemp       ("#5ac8fa");   // 拖拽中的临时线

// 应用调色板 + 样式表
void apply(QApplication &app);

}  // namespace FlowTheme
