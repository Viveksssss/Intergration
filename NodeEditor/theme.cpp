#include "theme.h"

#include <QFont>
#include <QPalette>

void Theme::apply(QApplication &app)
{
    app.setStyle("Fusion");

    QPalette p;
    p.setColor(QPalette::Window,           QColor(0x21,0x21,0x28));
    p.setColor(QPalette::WindowText,       QColor(0xe6,0xe6,0xf0));
    p.setColor(QPalette::Base,             QColor(0x1b,0x1b,0x21));
    p.setColor(QPalette::AlternateBase,    QColor(0x22,0x22,0x2a));
    p.setColor(QPalette::Text,             QColor(0xe6,0xe6,0xf0));
    p.setColor(QPalette::Button,           QColor(0x2b,0x2b,0x35));
    p.setColor(QPalette::ButtonText,       QColor(0xe6,0xe6,0xf0));
    p.setColor(QPalette::Highlight,        QColor(0x4d,0xa3,0xff));
    p.setColor(QPalette::HighlightedText,  QColor(0xff,0xff,0xff));
    p.setColor(QPalette::ToolTipBase,      QColor(0x2b,0x2b,0x35));
    p.setColor(QPalette::ToolTipText,      QColor(0xe6,0xe6,0xf0));
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(0x5f,0x5f,0x6b));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x5f,0x5f,0x6b));
    app.setPalette(p);

    QFont f("Microsoft YaHei UI");
    f.setPointSize(9);
    app.setFont(f);

    app.setStyleSheet(R"(
        QToolBar { background: #26262f; border: 0; spacing: 6px; padding: 6px; }
        QToolBar QLabel { color: #8f8f9d; padding: 0 4px; }
        QToolButton {
            color: #d8d8e4; background: #32323e;
            border: 1px solid #3f3f4d; border-radius: 5px;
            padding: 6px 12px;
        }
        QToolButton:hover   { background: #3d3d4b; border-color: #4da3ff; }
        QToolButton:pressed { background: #2a2a34; }
        QStatusBar { background: #26262f; color: #8f8f9d; }
        QStatusBar::item { border: 0; }
    )");
}
