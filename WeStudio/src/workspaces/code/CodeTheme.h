#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QApplication;

// ---------------------------------------------------------------------------
//  所有颜色 / 字体集中在这一个文件。
//  想换配色只改这里，不用去翻绘制代码。
//
//  这些颜色同时被三处使用：
//    1. CodeHighlighter  —— 语法着色
//    2. CodeEditor       —— 背景、当前行、括号匹配、折叠提示
//    3. LineNumberArea   —— 行号栏
// ---------------------------------------------------------------------------
namespace CodeTheme {

// ── 编辑区
inline const QColor kEditorBg      ("#1e1e24");   // 代码背景
inline const QColor kEditorFg      ("#d4d4dc");   // 普通文字
inline const QColor kCurrentLine   ("#2a2a35");   // 当前行底色（非常接近背景，只差一点）
inline const QColor kSelectionBg   ("#2f4a6b");   // 选中文字
inline const QColor kCaret         ("#ffd479");   // 光标

// ── 行号栏
inline const QColor kGutterBg      ("#191920");
inline const QColor kGutterBorder  ("#34343f");   // 行号栏右侧竖线
inline const QColor kGutterText    ("#5a5a6a");   // 普通行号
inline const QColor kGutterCurrent ("#c8c8d8");   // 当前行行号
inline const QColor kFoldArrow     ("#7a7a90");   // 折叠三角
inline const QColor kFoldArrowHot  ("#4da3ff");   // 三角悬停

// ── 语法着色（One Dark 系）
inline const QColor kKeyword       ("#c678dd");   // if / class / return
inline const QColor kType          ("#e5c07b");   // 自定义类型 / 类名
inline const QColor kFunction      ("#61afef");   // 函数名
inline const QColor kNumber        ("#d19a66");   // 数字
inline const QColor kString        ("#98c379");   // 字符串
inline const QColor kComment       ("#5c6370");   // 注释
inline const QColor kPreproc       ("#e06c75");   // #include / #define
inline const QColor kOperator      ("#56b6c2");   // + - = :: 等

// ── 括号匹配
inline const QColor kMatchBg       ("#3a4a5a");   // 匹配到的括号底色
inline const QColor kMatchFg       ("#ffd479");   // 匹配到的括号字色

// ── 折叠后的占位提示
inline const QColor kFoldHintBg    ("#2b2b36");
inline const QColor kFoldHintFg    ("#8a8a9d");

QFont  monoFont(int pointSize = 11);
void   apply(QApplication &app);

}  // namespace CodeTheme
