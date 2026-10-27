#pragma once

#include <QWidget>

class QFileSystemModel;
class QTreeView;
class QLabel;
class QToolButton;
class QLineEdit;

// ---------------------------------------------------------------------------
//  FileExplorer —— 左侧「资源管理器」面板
//
//  职责非常单一：显示一个文件夹树，用户点文件就发一个信号出去。
//  它不负责读文件、不负责开标签页，那些是 MainWindow 的事。
//  这样这个类可以脱离 CodeEditor 单独复用。
//
//  结构：
//      ┌──────────────────────────┐
//      │ 标题行  [文件夹名]  [⤢]  │  <- header：名字 + 折叠按钮
//      ├──────────────────────────┤
//      │ 过滤框（可选，默认隐藏） │  <- filter：按文件名过滤
//      ├──────────────────────────┤
//      │ ▾ src                    │
//      │   ▸ 子目录               │  <- QTreeView + QFileSystemModel
//      │     main.cpp             │
//      └──────────────────────────┘
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

signals:
    // 用户单击/双击了一个文件（已是绝对路径）
    void fileActivated(const QString &filePath);
    // 用户要求重新选一个文件夹（标题栏上的按钮）
    void openFolderRequested();

private slots:
    void onTreeClicked(const QModelIndex &index);
    void onTreeDoubleClicked(const QModelIndex &index);
    void onFilterChanged(const QString &text);

private:
    void buildUi();
    void applyFilter(const QString &text);
    void switchToIndex(const QModelIndex &index);

    QFileSystemModel *_model  = nullptr;
    QTreeView        *_tree   = nullptr;
    QLabel           *_title  = nullptr;
    QToolButton      *_pickBtn = nullptr;
    QLineEdit        *_filter = nullptr;
    QString           _rootPath;
};
