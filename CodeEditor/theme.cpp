#include "theme.h"

#include <QFontDatabase>
#include <QPalette>
#include <qapplication.h>
#include <QStyleFactory>
namespace Theme{

QFont monoFont(int pointSize)
{
    const QStringList candidates = {
        "Cascadia Mono", "JetBrains Mono", "Consolas", "DejaVu Sans Mono", "Courier New"
    };
    const QStringList families = QFontDatabase::families();
    for(const auto&name:candidates){
        if (families.contains(name)){
            QFont f(name,pointSize);
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

}
