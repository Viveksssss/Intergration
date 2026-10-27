#pragma once

#include <QWidget>

class QFileSystemModel;
class QTreeView;
class QLabel;
class QToolButton;
class QLineEdit;
class QModelIndex;

// ---------------------------------------------------------------------------
//  FileExplorer —— 左侧「资源管理器」面板
//
//  职责：显示一个文件夹树，用户点文件就发信号；右键提供新建/删除/重命名/属性。
//  它不负责读文件、不负责开标签页，那些是 MainWindow 的事。
//
//  结构：
//      ┌──────────────────────────┐
//      │ 标题行  [文件夹名]  [⤢]  │  <- header：名字 + 打开文件夹
//      ├──────────────────────────┤
//      │ 过滤框（可选，默认隐藏） │
//      ├──────────────────────────┤
//      │ ▾ src                    │
//      │   ▸ 子目录               │  <- QTreeView + QFileSystemModel
//      │     main.cpp             │
//      └──────────────────────────┘
//
//  右键菜单（在树视图中）：
//      新建文件 / 新建文件夹 / 重命名 / 删除 / 在资源管理器中显示 / 属性 / 刷新
//  快捷键：F2 重命名、Delete 删除
// ---------------------------------------------------------------------------
class FileExplorer : public QWidget
{
    Q_OBJECT

public:
    explicit FileExplorer(QWidget *parent = nullptr);

    // 打开一个文件夹（rootPath），失败返回 false
    bool openFolder(const QString &dirPath);

    QString rootPath() const { return _rootPath; }
    void    setFilterVisible(bool on);
    void    setHeaderText(const QString &text);

    // 让某个路径在树里被选中并展开父级（重命名/新建后定位用）
    void revealPath(const QString &path);

    // 当前选中的路径（可多选，返回全部）
    QStringList selectedPaths() const;

    // 触发一次「原位重命名」编辑（外部 F2 / 菜单调用）
    void renameCurrent();

signals:
    // 用户单击/双击了一个文件（已是绝对路径）
    void fileActivated(const QString &filePath);
    // 用户要求重新选一个文件夹（标题栏上的按钮）
    void openFolderRequested();

    // ── 供外壳同步已打开的标签页
    void fileRenamed(const QString &oldPath, const QString &newPath);
    void pathDeleted(const QString &path);

private slots:
    void onTreeClicked(const QModelIndex &index);
    void onTreeDoubleClicked(const QModelIndex &index);
    void onFilterChanged(const QString &text);
    void onCommitRename(const QString &newName);

private:
    void buildUi();
    void applyFilter(const QString &text);
    void switchToIndex(const QModelIndex &index);

    // 右键菜单相关
    void showContextMenu(const QPoint &globalPos);
    QString  targetDirForNew() const;      // 新建时的落点目录
    void     doNewFile();
    void     doNewFolder();
    void     doDelete();
    void     doProperties();

    QFileSystemModel *_model  = nullptr;
    QTreeView        *_tree   = nullptr;
    QLabel           *_title  = nullptr;
    QToolButton      *_pickBtn = nullptr;
    QLineEdit        *_filter = nullptr;
    QString           _rootPath;

    // 重命名（委托）过程中的临时状态
    QString           _renamingPath;
};
