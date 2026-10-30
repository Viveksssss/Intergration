#pragma once

#include <QColor>
#include <QFont>
#include <QRect>

// ===========================================================================
//  配色、尺寸、字体集中在这里。
//
//  看板这种界面全是自绘（圆角卡片、列头、占位符、拖拽阴影），
//  颜色散在各处就没法统一调。收口到一个头文件，改主题只改这里。
// ===========================================================================
namespace BoardTheme {

// ---- 画布 / 列 / 卡片 ----
inline const QColor kBoardBg     ("#11131a");   // 窗口底色
inline const QColor kColumnBg    ( 26,  29,  38);
inline const QColor kColumnBgHot ( 30,  34,  46);   // 拖动悬停时略亮
inline const QColor kColumnBorder( 44,  48,  62);
inline const QColor kCardBg      ("#1e222c");
inline const QColor kCardBgHover ("#252a36");
inline const QColor kCardBorder  ( 48,  53,  68);
inline const QColor kSlotBg      (255, 255, 255, 12);   // 占位符底色

// ---- 文字 ----
inline const QColor kText        ("#e8eaf2");
inline const QColor kTextDim     ("#8d93a6");
inline const QColor kCountBg     (255, 255, 255, 18);

// ---- 强调色（标签） ----
inline const QColor kTagBlue     ("#2ea8ff");
inline const QColor kTagGreen    ("#34c759");
inline const QColor kTagOrange   ("#ff9f0a");
inline const QColor kTagPurple   ("#bf6bf0");
inline const QColor kTagRed      ("#ff453a");

// ---- 尺寸（全部是逻辑像素） ----
inline constexpr int kColumnWidth   = 300;
inline constexpr int kColumnGap     = 18;
inline constexpr int kColumnPad     = 12;    // 列内左右内边距
inline constexpr int kCardGap       = 10;    // 卡片之间的垂直间距
inline constexpr int kCardHeight    = 76;
inline constexpr int kCardRadius    = 10;
inline constexpr int kColumnRadius  = 14;
inline constexpr int kHeaderHeight  = 46;
inline constexpr int kEdgeScrollY   = 56;    // 上下边缘多少像素内开始自动滚动

QFont uiFont(int pt, bool bold = false);
QFont monoFont(int pt);

}  // namespace BoardTheme
