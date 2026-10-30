// ============================================================================
//  TimelineTheme.h —— 配色与全局样式
// ----------------------------------------------------------------------------
//  这个文件存在的唯一理由：让「颜色」只有一个来源。
//  想换色调（比如做个浅色版、或者模仿 PR/Final Cut 的紫灰色），
//  改这里就够了，不用去绘制代码里一个个翻 QColor("#...")。
// ============================================================================
#pragma once

#include <QColor>

class QApplication;

// 全部颜色收在一个命名空间里，写法上 TimelineTheme::kPlayhead 一眼知道是干什么的
namespace TimelineTheme {

// ---- 背景 / 面板 ----
inline const QColor kWindow     ("#1b1b1f");   // 主窗口底色
inline const QColor kBase       ("#141418");   // 时间轴画布底色（比窗口更深一层）
inline const QColor kPanel      ("#26262d");   // 工具栏按钮、控件表面
inline const QColor kTrackBg    ("#24242b");   // 轨道条底槽

// ---- 文字 ----
inline const QColor kText       ("#dcdce4");   // 主文字
inline const QColor kTextDim    ("#9a9aa8");   // 刻度数字
inline const QColor kTextFaint  ("#6f6f7c");   // 底部状态栏文字
inline const QColor kTextMuted  (255, 255, 255, 150);  // 轨道上的标题文字

// ---- 视频条 ----
inline const QColor kClipTop    ("#325376");   // 渐变上端
inline const QColor kClipBottom ("#22344d");   // 渐变下端
inline const QColor kClipBorder ("#4a7cb5");   // 描边
inline const QColor kPlayedMask (64, 158, 255, 55);    // 已播放部分的高亮遮罩

// ---- 交互元素 ----
inline const QColor kAccent     ("#4da3ff");   // 主色：框选框
inline const QColor kSelFill    (77, 163, 255, 45);    // 框选内部填充
inline const QColor kPlayhead   ("#ff5252");   // 播放头（红线）

// ---- 细线（带透明度，所以用 RGBA）----
inline const QColor kCellLine   (255, 255, 255, 18);   // 帧格短线
inline const QColor kGridLine   (255, 255, 255, 26);   // 刻度长线

}  // namespace TimelineTheme

// 把深色主题应用到整个 app：调色板 + 一段样式表。
// 放在 .cpp 里是因为它属于「启动时做一次」的事，不需要每个编译单元都看到实现。
void applyDarkTheme(QApplication &app);
