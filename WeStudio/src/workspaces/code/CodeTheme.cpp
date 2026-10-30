#include "CodeTheme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

namespace CodeTheme {

QFont monoFont(int pointSize)
{
    // 优先用这几款等宽字体，谁先装上用谁。
    // 等宽是代码编辑器的硬性要求：行号栏靠「字符宽度 × 位数」算宽度，
    // 如果字体不等宽，数字 1 比 8 窄，行号就会左右抖。
    const QStringList candidates = {
        "Cascadia Mono", "JetBrains Mono", "Consolas", "DejaVu Sans Mono", "Courier New"
    };
    const QStringList families = QFontDatabase::families();
    for (const QString &name : candidates) {
        if (families.contains(name)) {
            QFont f(name, pointSize);
            f.setStyleHint(QFont::Monospace);
            f.setFixedPitch(true);
            return f;
        }
    }
    QFont f;
    f.setFamily("monospace");
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    f.setPointSize(pointSize);
    return f;
}

void apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette p;
    p.setColor(QPalette::Window,          kEditorBg);
    p.setColor(QPalette::WindowText,      kEditorFg);
    p.setColor(QPalette::Base,            kEditorBg);
    p.setColor(QPalette::AlternateBase,   kCurrentLine);
    p.setColor(QPalette::Text,            kEditorFg);
    p.setColor(QPalette::Button,          kGutterBg);
    p.setColor(QPalette::ButtonText,      kEditorFg);
    p.setColor(QPalette::Highlight,       kSelectionBg);
    p.setColor(QPalette::HighlightedText, kEditorFg);
    p.setColor(QPalette::ToolTipBase,     kCurrentLine);
    p.setColor(QPalette::ToolTipText,     kEditorFg);
    p.setColor(QPalette::Disabled, QPalette::Text, kGutterText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, kGutterText);
    app.setPalette(p);

    app.setStyleSheet(QString(R"(
        QToolBar { background:#22222a; border:0; padding:3px; spacing:4px; }
        QToolButton { color:#c8c8d8; padding:4px 10px; border-radius:4px; }
        QToolButton:hover { background:#2f2f3a; }
        QToolButton:pressed { background:#3a3a47; }
        QStatusBar { background:#22222a; color:#8a8a9d; }
        QScrollBar:vertical { background:#181820; width:12px; margin:0; }
        QScrollBar::handle:vertical { background:#3a3a47; min-height:28px; border-radius:6px; }
        QScrollBar::handle:vertical:hover { background:#4a4a5c; }
        QScrollBar::add-line, QScrollBar::sub-line { height:0; }
        QScrollBar:horizontal { background:#181820; height:12px; margin:0; }
        QScrollBar::handle:horizontal { background:#3a3a47; min-width:28px; border-radius:6px; }
    )"));
}

}  // namespace CodeTheme
