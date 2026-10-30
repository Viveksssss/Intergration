#pragma once

#include <QMainWindow>

class CodeEditor;
class QLabel;

// ---------------------------------------------------------------------------
//  外壳窗口：工具栏 + 状态栏 + 中间的 CodeEditor。
//  真正有技术含量的东西全在 CodeEditor 和它的小伙伴里，这里只做组装。
// ---------------------------------------------------------------------------
class CodePage : public QMainWindow
{
    Q_OBJECT

public:
    explicit CodePage(QWidget *parent = nullptr);

    // 给 main.cpp 的自检参数用（--fold / --caret）
    CodeEditor *editor() const { return m_editor; }

private slots:
    void openFile();
    void jumpToLine();
    void updateStatus(int line, int column);
    void updateFoldActions();

private:
    void buildToolBar();
    void buildStatusBar();
    void loadDemoDocument();

    CodeEditor *m_editor    = nullptr;
    QLabel     *m_posLabel  = nullptr;
    QLabel     *m_statLabel = nullptr;
    QAction    *m_foldAllAct   = nullptr;
    QAction    *m_unfoldAllAct = nullptr;
};
