#include "fileexplorer.h"
#include "theme.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

FileExplorer::FileExplorer(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

// ---------------------------------------------------------------------------
//  UI 组装
// ---------------------------------------------------------------------------
void FileExplorer::buildUi()
{
    setObjectName(QStringLiteral("fileExplorer"));
    setMinimumWidth(150);

    _title = new QLabel(QStringLiteral("资源管理器"), this);
    _title->setObjectName(QStringLiteral("explorerTitle"));

    _pickBtn = new QToolButton(this);
    _pickBtn->setText(QStringLiteral("⤢"));
    _pickBtn->setToolTip(QStringLiteral("打开文件夹"));
    _pickBtn->setCursor(Qt::PointingHandCursor);
    _pickBtn->setAutoRaise(true);
    connect(_pickBtn, &QToolButton::clicked, this, &FileExplorer::openFolderRequested);

    QWidget *header = new QWidget(this);
    header->setObjectName(QStringLiteral("explorerHeader"));
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 6, 4, 6);
    hl->setSpacing(4);
    hl->addWidget(_title, 1);
    hl->addWidget(_pickBtn, 0);

    // ---- 文件系统模型
    //  注意：QFileSystemModel 内部有自己的工作线程，dirLoaded 信号是异步来的，
    //  所以「打开后立刻展开第一层」必须等信号，不能在 setRootPath() 后面直接 expand。
    _model = new QFileSystemModel(this);
    _model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Drives);
    _model->setReadOnly(true);
    _model->setNameFilterDisables(false);   // 被过滤掉的东西直接不显示，而不是变灰
    _model->setNameFilters(QStringList() << QStringLiteral("*"));

    _tree = new QTreeView(this);
    _tree->setModel(_model);
    _tree->setObjectName(QStringLiteral("explorerTree"));
    _tree->setHeaderHidden(true);
    _tree->setAnimated(true);
    _tree->setUniformRowHeights(true);
    _tree->setIndentation(14);
    _tree->setExpandsOnDoubleClick(false);  // 双击 = 打开文件，展开交给单击箭头
    _tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 只显示第一列（文件名列），把大小/类型/日期列藏起来
    for (int c = 1; c < _model->columnCount(); ++c)
        _tree->hideColumn(c);

    QObject::connect(_tree, &QTreeView::clicked,        this, &FileExplorer::onTreeClicked);
    QObject::connect(_tree, &QTreeView::doubleClicked,  this, &FileExplorer::onTreeDoubleClicked);

    // 懒加载第一层：目录读完后自动把根节点展开
    QObject::connect(_model, &QFileSystemModel::directoryLoaded,
                     this, [this](const QString &path) {
        if (_rootPath.isEmpty())
            return;
        if (QDir(path) == QDir(_rootPath)) {
            const QModelIndex root = _model->index(path);
            _tree->setExpanded(root, true);
        }
    });

    // ---- 过滤框（默认隐藏，以后想开就 setFilterVisible(true)）
    _filter = new QLineEdit(this);
    _filter->setPlaceholderText(QStringLiteral("按名称过滤…"));
    _filter->setClearButtonEnabled(true);
    _filter->setVisible(false);
    QObject::connect(_filter, &QLineEdit::textChanged, this, &FileExplorer::onFilterChanged);

    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);
    vl->addWidget(header);
    vl->addWidget(_filter);
    vl->addWidget(_tree, 1);
}

// ---------------------------------------------------------------------------
//  打开文件夹
// ---------------------------------------------------------------------------
bool FileExplorer::openFolder(const QString &dirPath)
{
    const QFileInfo info(dirPath);
    if (!info.exists() || !info.isDir())
        return false;

    _rootPath = info.absoluteFilePath();

    // setRootPath 之后模型换根；根节点本身会被显示成一行，所以把它藏掉
    const QModelIndex root = _model->setRootPath(_rootPath);
    _tree->setRootIndex(root);
    _tree->setExpanded(root, true);

    setHeaderText(QFileInfo(_rootPath).fileName());
    _filter->clear();
    return true;
}

void FileExplorer::setFilterVisible(bool on)
{
    _filter->setVisible(on);
}

void FileExplorer::setHeaderText(const QString &text)
{
    _title->setText(text);
    _title->setToolTip(text);
}

// ---------------------------------------------------------------------------
//  交互
//
//  单击：目录 -> 展开/收起；文件 -> 打开到标签页
//  双击：文件 -> 打开（在已经打开的标签页上按名字激活）
// ---------------------------------------------------------------------------
void FileExplorer::onTreeClicked(const QModelIndex &index)
{
    switchToIndex(index);

    if (_model->isDir(index))
        return;                                     // 目录：交给默认的展开行为

    const QString path = _model->filePath(index);
    if (!path.isEmpty())
        emit fileActivated(path);
}

void FileExplorer::onTreeDoubleClicked(const QModelIndex &index)
{
    if (_model->isDir(index)) {
        _tree->setExpanded(index, !_tree->isExpanded(index));
        return;
    }
    const QString path = _model->filePath(index);
    if (!path.isEmpty())
        emit fileActivated(path);
}

void FileExplorer::switchToIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    _tree->setCurrentIndex(index);
    _tree->scrollTo(index);
}

// ---------------------------------------------------------------------------
//  过滤
//
//  这里不用 QSortFilterProxyModel，因为代理层会打断 QFileSystemModel 的
//  「某个节点是不是懒加载过」的状态，把整个树拉成递归扫描。
//  代价是这个过滤只看「已经加载出来的节点」，对本地小工程够用。
// ---------------------------------------------------------------------------
void FileExplorer::applyFilter(const QString &text)
{
    const QString needle = text.trimmed();
    _model->setNameFilters(needle.isEmpty()
                           ? QStringList() << QStringLiteral("*")
                           : QStringList() << QStringLiteral("*%1*").arg(needle));
    if (!needle.isEmpty()) {
        _tree->expandAll();
    }
}

void FileExplorer::onFilterChanged(const QString &text)
{
    applyFilter(text);
}
