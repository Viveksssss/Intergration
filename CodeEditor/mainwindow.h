#pragma once

#include <QMainWindow>

class CodeEditor;
class QLabel;

// ---------------------------------------------------------------------------
//  外壳窗口：工具栏 + 状态栏 + 中间的 CodeEditor。
//  真正有技术含量的东西全在 CodeEditor 和它的小伙伴里，这里只做组装。
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 给 main.cpp 的自检参数用（--fold / --caret）
    CodeEditor *editor() const { return _editor; }

private slots:
    void openFile();
    void jumpToLine();
    void updateStatus(int line, int column);
    void updateFoldActions();
    void saveFile();

private:
    void buildToolBar();
    void buildStatusBar();
    void loadDemoDocument();

    CodeEditor *_editor    = nullptr;
    QLabel     *_posLabel  = nullptr;
    QLabel     *_statLabel = nullptr;
    QAction    *_foldAllAct   = nullptr;
    QAction    *_unfoldAllAct = nullptr;
    bool _openFromFile = false;
    QString _currentPath;   // 记录当前文件路径，另存/直接保存用
};
