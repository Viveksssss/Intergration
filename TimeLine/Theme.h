#ifndef THEME_H
#define THEME_H


#include <QColor>
#include <QApplication>
namespace Theme{
inline const QColor WINDOW      (0x1b,0x1b,0x1f);   // 主窗口底色
inline const QColor BASE        (0x14,0x14,0x18);   // 时间轴背景
inline const QColor TRACKBG     (0x24,0x24,0x2b);   // 轨道底槽
inline const QColor PANEL       (0x26,0x26,0x2d);   // 工具栏按钮、控件表面

inline const QColor PLAYERMASK  (0x4a,0x7c,0xff,55);// 已播放部分的高亮这招
inline const QColor CLIPTOP     (0x32,0x53,0x76);   // 视频条渐变上端
inline const QColor CLIPBOTTOM  (0x22,0x34,0x4d);   // 视频条渐变下端
inline const QColor CLIPBORDER  (0x4a,0x7c,0xb5);   // 视频条描边

inline const QColor TEXT        (0xdc,0xdc,0xe4);   // 主文字
inline const QColor TEXTFAINT   (0x6f,0x6f,0x7c);   // 底部状态栏文字
inline const QColor TEXTMUTED   (0xc8,0xc8,0xd4);   // 轨道上的文字
inline const QColor TEXTDIM     (0x9a,0x9a,0xa8);   // 刻度文字

inline const QColor CELLLINE    (0xff,0xff,0xff,18);// 帧格短线
inline const QColor GRIDLINE    (0xFF,0xFF,0xFF,26);// 网格线：白色 10% 透明

inline const QColor ACCENT      (0x4d,0xa3,0xff);   // 主色框选框
inline const QColor SELFILL     (77, 163, 255, 45); // 框选内部填充
inline const QColor PLAYHEAD    (0xff,0x52,0x52);   // 播放头（红线）
}

void applyDarkTheme(QApplication &app);
#endif // THEME_H
