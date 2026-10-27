#pragma once

#include <QMainWindow>

class CodeEditor;
class EditorTabs;
class FileExplorer;
class QCloseEvent;
class QLabel;
class QSplitter;
class QWidget;

// ---------------------------------------------------------------------------
//  外壳窗口：工具栏 + 左侧资源管理器 + 中间标签页编辑器 + 状态栏。
//  真正有技术含量的东西全在 CodeEditor 和它的小伙伴里，这里只做组装。
//
//  布局：
//      ┌──────────────────────────────────────────────┐
//      │ 工具栏（打开文件 / 保存 / 跳转 / 折叠 / 换行）│
//      ├────────────┬─────────────────────────────────┤
//      │ 资源管理器 │  Tab1 | Tab2 | Tab3        [+]  │
//      │  (树)      ├─────────────────────────────────┤
//      │            │  CodeEditor                     │
//      ├────────────┴─────────────────────────────────┤
//      │ 状态栏：行/列                     行数/字符数 │
//      └──────────────────────────────────────────────┘
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 给 main.cpp 的自检参数用（--fold / --caret）
    // 返回「当前标签页里的编辑器」，空标签页时返回 nullptr
    CodeEditor *editor() const { return _editor; }

    // 供 main.cpp 打开命令行传入的文件（走标签页，不再直接 setPlainText）
    void openPath(const QString &path);
    // 供 main.cpp 用内容新建一个标签（未命名）
    void openContent(const QString &title, const QString &text);

protected:
    // 关窗口时逐个标签页确认未保存修改（不能写在析构里，见文档 4.2(2)）
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openFile();
    void openFolder();
    void jumpToLine();
    void updateStatus(int line, int column);
    void updateFoldActions();
    void saveFile();

    // ── 新增：标签页 / 资源管理器的联动
    void onEditorChanged(CodeEditor *editor);
    void onFileActivated(const QString &path);
    void toggleExplorer();

private:
    void buildToolBar();
    void buildStatusBar();
    void buildCentralArea();
    void buildShortcuts();
    void loadDemoDocument();

    // 当前编辑器变了：重新接信号、刷新标题/折叠按钮/状态栏
    void attachEditor(CodeEditor *editor);
    void resetEditorState();

    EditorTabs   *_tabs     = nullptr;
    FileExplorer *_explorer = nullptr;
    QSplitter    *_splitter = nullptr;
    QWidget      *_emptyPage = nullptr;   // 没有打开任何文件时的占位

    CodeEditor *_editor    = nullptr;     // 指向「当前标签页的编辑器」，可能为空
    CodeEditor *_connected = nullptr;     // 当前已经连过信号的那个编辑器

    QLabel     *_posLabel  = nullptr;
    QLabel     *_statLabel = nullptr;
    QAction    *_foldAllAct   = nullptr;
    QAction    *_unfoldAllAct = nullptr;
    QAction    *_explorerAct  = nullptr;
    bool _openFromFile = false;
};
