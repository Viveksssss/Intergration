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

        /* ---------- 新增：左侧资源管理器 ---------- */
        QWidget#fileExplorer { background:#191920; }
        QWidget#explorerHeader { background:#1c1c24; border-bottom:1px solid #2c2c36; }
        QLabel#explorerTitle {
            color:#9a9aac; font-size:11px; font-weight:bold; letter-spacing:1px;
        }
        QTreeView#explorerTree {
            background:#191920; border:0; outline:0; color:#c8c8d8;
            show-decoration-selected:1; padding:2px 0;
        }
        QTreeView#explorerTree::item { padding:3px 2px; border:0; }
        QTreeView#explorerTree::item:hover { background:#24242e; }
        QTreeView#explorerTree::item:selected { background:#2f4a6b; color:#ffffff; }
        QTreeView#explorerTree::branch { background:transparent; }

        /* ---------- 新增：顶部标签页栏 ---------- */
        QTabWidget#editorTabs::pane {
            border:0; border-top:1px solid #2c2c36; background:#1e1e24;
        }
        QTabWidget#editorTabs QTabBar::tab {
            background:#1a1a21; color:#8a8a9d;
            padding:6px 12px; margin-right:1px; border:0;
            border-top:2px solid transparent; min-width:70px;
        }
        QTabWidget#editorTabs QTabBar::tab:hover { background:#24242e; color:#c8c8d8; }
        QTabWidget#editorTabs QTabBar::tab:selected {
            background:#1e1e24; color:#e6e6f0; border-top:2px solid #4da3ff;
        }
        /* 关闭按钮由 EditorTabs 自绘（见 editortabs.cpp 的 CloseButton），
           这里不能再写 ::close-button 的 image:none，否则会把图标抹掉。 */
        QTabWidget#editorTabs QTabBar::tab { padding-right: 6px; }

        /* ---------- 新增：空白页 ---------- */
        QWidget#blankPage { background:#1e1e24; }
        QLabel#blankTitle { color:#4a4a5c; font-size:22px; font-weight:bold; }
        QLabel#blankHint  { color:#5a5a6a; font-size:12px; }

        /* ---------- 新增：分隔条 ---------- */
        QSplitter#mainSplitter::handle { background:#2c2c36; }
        QSplitter#mainSplitter::handle:hover { background:#4da3ff; }
    )"));
}

}
