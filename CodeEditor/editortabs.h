#pragma once

#include <QTabWidget>

class CodeEditor;

// ---------------------------------------------------------------------------
//  EditorTabs —— 顶部标签页栏 + 多份编辑器
//
//  每个打开的标签就是一个独立的 CodeEditor 实例（自带语法高亮、折叠、
//  括号匹配、行号栏 —— 这些能力一个都不用改）。
//
//  设计要点：
//   1. 每个 CodeEditor 用 setProperty() 挂了 "filePath" / "savedText"，
//      脏标记靠「当前文本 != 上次保存的文本」判定，不需要给 CodeEditor
//      再加成员变量，也就不必改原有类。
//   2. 空态时（一个标签都没有）放一个占位 QWidget，MainWindow 依旧能拿到
//      一个合法的当前编辑器指针，原有逻辑（状态栏、折叠按钮）不会崩。
//   3. currentChanged / tabCloseRequested / document 的 modifiedChanged
//      统一转成对外信号，界面上的事 MainWindow 不用管。
// ---------------------------------------------------------------------------
class EditorTabs : public QTabWidget
{
    Q_OBJECT

public:
    explicit EditorTabs(QWidget *parent = nullptr);

    // 打开文件；已经打开过就切过去。失败返回 nullptr
    CodeEditor *openFile(const QString &filePath);

    // 用内容直接新建一个未命名标签（main.cpp 的 --open 走这里）
    CodeEditor *openContent(const QString &title, const QString &text,
                            const QString &filePath = QString());

    QWidget *blankPage() const { return _blank; }

    CodeEditor *currentEditor() const;
    CodeEditor *editorAt(int index) const;
    QString     currentFilePath() const;          // 空标签/未命名返回 ""
    QString     filePathOf(int index) const;

    // 文档是否与「上次保存的内容」不一致
    bool isDirty(int index) const;

    // 把当前编辑器的内容写回 its filePath，没有路径返回 false（需要另存为）
    bool saveIndex(int index, const QString &path = QString());

    // 关闭；脏的会弹窗问，用户取消返回 false
    bool maybeClose(int index);
    // 批量关闭（从大到小删，避免下标错位）
    bool closeOthers(int keepIndex);
    bool closeRight(int fromIndex);
    void closeAll();
    // 关闭当前页（Ctrl+W）
    void closeCurrent();

    // 还剩几个「真正有编辑器」的标签（不含欢迎页）
    int editorCount() const;

    int indexOfFile(const QString &filePath) const;

    // ── 资源管理器改动文件后，同步已打开的标签
    // 重命名：把该路径的标签换成新路径（标签文字也跟着变）
    void notifyFileRenamed(const QString &oldPath, const QString &newPath);
    // 删除：把该路径（及其子路径）的标签直接关掉，不弹保存提示
    void notifyPathDeleted(const QString &path);

    void setCurrentEditor(CodeEditor *editor);
    void refreshTabText(int index);

signals:
    // 当前标签换了（空 -> 有 / 有 -> 空 都会发）
    void currentEditorChanged(CodeEditor *editor);
    // 标签标题（脏标记）变化
    void editorDirtyChanged(CodeEditor *editor, bool dirty);
    // 在空白页上点了内容（比如提示「打开文件夹」），外壳可以响应
    void blankPageClicked();
    // 所有标签都被关掉了
    void allTabsClosed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onCurrentChanged(int index);
    void onCloseRequested(int index);
    void showTabMenu(const QPoint &pos);

private:
    void connectEditor(CodeEditor *editor);
    int  indexOfEditor(CodeEditor *editor) const;
    void updateBlankState();
    // 重新给每个标签挂上自绘关闭按钮（下标变了就必须重挂）
    void renumberCloseButtons();

    QWidget *_blank = nullptr;
};
