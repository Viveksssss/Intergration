#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QApplication;


namespace Theme{

// ── 编辑区
inline const QColor kEditorBg      (0x1e,0x1e,0x24);   // 代码背景
inline const QColor kEditorFg      (0xd4,0xd4,0xdc);   // 普通文字
inline const QColor kCurrentLine   (0x2a,0x2a,0x35);   // 当前行底色（非常接近背景，只差一点）
inline const QColor kSelectionBg   (0x2f,0x4a,0x6b);   // 选中文字
inline const QColor kCaret         (0xff,0xd4,0x79);   // 光标

// ── 行号栏
inline const QColor kGutterBg      (0x19,0x19,0x20);
inline const QColor kGutterBorder  (0x34,0x34,0x3f);   // 行号栏右侧竖线
inline const QColor kGutterText    (0x5a,0x5a,0x6a);   // 普通行号
inline const QColor kGutterCurrent (0xc8,0xc8,0xd8);   // 当前行行号
inline const QColor kFoldArrow     (0x7a,0x7a,0x90);   // 折叠三角
inline const QColor kFoldArrowHot  (0x4d,0xa3,0xff);   // 三角悬停

// ── 语法着色（One Dark 系）
inline const QColor kKeyword       (0xc6,0x78,0xdd);   // if / class / return
inline const QColor kType          (0xe5,0xc0,0x7b);   // 自定义类型 / 类名
inline const QColor kFunction      (0x61,0xaf,0xef);   // 函数名
inline const QColor kNumber        (0xd1,0x9a,0x66);   // 数字
inline const QColor kString        (0x98,0xc3,0x79);   // 字符串
inline const QColor kComment       (0x5c,0x63,0x70);   // 注释
inline const QColor kPreproc       (0xe0,0x6c,0x75);   // #include / #define
inline const QColor kOperator      (0x56,0xb6,0xc2);   // + - = :: 等

// ── 括号匹配
inline const QColor kMatchBg       (0x3a,0x4a,0x5a);   // 匹配到的括号底色
inline const QColor kMatchFg       (0xff,0x11,0x00);   // 匹配到的括号字色

// ── 折叠后的占位提示
inline const QColor kFoldHintBg    (0x2b,0x2b,0x36);
inline const QColor kFoldHintFg    (0x8a,0x8a,0x9d);

QFont  monoFont(int pointSize = 11);
void   apply(QApplication &app);



}
