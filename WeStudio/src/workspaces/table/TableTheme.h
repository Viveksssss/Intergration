#pragma once

#include <QColor>
#include <QFont>

// ===========================================================================
//  配色、尺寸、字体集中在这里。
//
//  这张表全是自绘（斑马纹、进度条、状态点、多行文本），
//  颜色散在各 delegate 里就没法统一调。收口到一个头文件，改主题只改这里。
// ===========================================================================
namespace TableTheme {

// ---- 画布 / 表格 ----
inline const QColor kWindowBg    ("#11131a");   // 窗口底色
inline const QColor kViewBg      ("#171a22");   // 表格视口底色
inline const QColor kRowEven     ("#171a22");   // 偶数行
inline const QColor kRowOdd      ("#1b1f28");   // 奇数行(斑马纹)
inline const QColor kRowHover    ( 40,  45,  58);   // 鼠标悬停行
inline const QColor kRowSelected ( 33,  60,  92);   // 选中行底色
inline const QColor kGridLine    ( 38,  42,  54);

// ---- 文字 ----
inline const QColor kText        ("#e8eaf2");
inline const QColor kTextDim     ("#8d93a6");
inline const QColor kHeaderBg    ("#1d212a");
inline const QColor kHeaderText  ("#b9c0d4");

// ---- 强调色 ----
inline const QColor kAccent      ("#2ea8ff");
inline const QColor kGreen       ("#34c759");
inline const QColor kOrange      ("#ff9f0a");
inline const QColor kRed         ("#ff453a");
inline const QColor kTrack       (255, 255, 255, 26);   // 进度条底槽

// ---- 尺寸(逻辑像素) ----
inline constexpr int kCellPadX     = 14;   // 单元格左右内边距
inline constexpr int kCellPadY     = 6;
inline constexpr int kBarHeight    = 10;   // 进度条厚度
inline constexpr int kBarRadius    = 5;
inline constexpr int kHeaderHeight = 38;
inline constexpr int kDefaultRowH  = 34;

QFont uiFont(int pt, bool bold = false);
QFont monoFont(int pt);

}  // namespace TableTheme
