#include "FlowTheme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>

namespace FlowTheme {

void apply(QApplication &app)
{
    // Fusion 是唯一能稳定响应调色板的风格。原生 Windows 风格会忽略大部分颜色。
    // 必须在 setPalette 之前设置，否则会触发一次风格重建把调色板冲掉。
    app.setStyle("Fusion");

    QPalette p;
    p.setColor(QPalette::Window,           QColor("#212128"));
    p.setColor(QPalette::WindowText,       QColor("#e6e6f0"));
    p.setColor(QPalette::Base,             QColor("#1b1b21"));
    p.setColor(QPalette::AlternateBase,    QColor("#22222a"));
    p.setColor(QPalette::Text,             QColor("#e6e6f0"));
    p.setColor(QPalette::Button,           QColor("#2b2b35"));
    p.setColor(QPalette::ButtonText,       QColor("#e6e6f0"));
    p.setColor(QPalette::Highlight,        QColor("#4da3ff"));
    p.setColor(QPalette::HighlightedText,  QColor("#ffffff"));
    p.setColor(QPalette::ToolTipBase,      QColor("#2b2b35"));
    p.setColor(QPalette::ToolTipText,      QColor("#e6e6f0"));
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor("#5f5f6b"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#5f5f6b"));
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

}  // namespace FlowTheme
