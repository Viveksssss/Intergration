#include <QPalette>
#include "Theme.h"

void applyDarkTheme(QApplication &app)
{
    app.setStyle("Fusion");

    QPalette p;
    p.setColor(QPalette::Window,          Theme::WINDOW);
    p.setColor(QPalette::WindowText,      Theme::TEXT);
    p.setColor(QPalette::Base,            Theme::BASE);
    p.setColor(QPalette::AlternateBase,   QColor("#202027"));
    p.setColor(QPalette::Text,            Theme::TEXT);
    p.setColor(QPalette::Button,          Theme::PANEL);
    p.setColor(QPalette::ButtonText,      Theme::TEXT);
    p.setColor(QPalette::Highlight,       QColor("#3d7ecc"));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase,     Theme::PANEL);
    p.setColor(QPalette::ToolTipText,     Theme::TEXT);
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor("#5f5f6b"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#5f5f6b"));
    app.setPalette(p);

    // 工具栏、状态栏这些控件的外框不是靠调色板画的，Fusion 不一定跟着变，
    // 补一小段样式表最省事（注意：这里只能写「框架」相关样式，
    // 一旦给 QPushButton 之类写背景，就会把调色板覆盖掉）。
    app.setStyleSheet(R"(
        QMainWindow, QToolBar, QStatusBar, QSplitter { background: #1b1b1f; }
        QToolBar { border: 0px; padding: 4px 8px; spacing: 2px; }
        QToolBar QToolButton { color: #dcdce4; padding: 5px 10px; border-radius: 4px; }
        QToolBar QToolButton:hover { background: #2e2e38; }
        QToolBar::separator { background: #33333d; width: 1px; margin: 6px; }
        QSplitter::handle { background: #2a2a32; }
        QSplitter::handle:hover { background: #3d7ecc; }
        QLabel { color: #8f8f9d; }
    )");
}
