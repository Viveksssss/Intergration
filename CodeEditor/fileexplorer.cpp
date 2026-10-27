#include "fileexplorer.h"
#include "fileops.h"
#include "theme.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <functional>

// ---------------------------------------------------------------------------
//  重命名委托
//
//  为什么不用 QFileSystemModel::setReadOnly(false) 让它自己改？
//  那样 Qt 会在 model 里直接 rename，但我们无从拦截「名字非法 / 目标已存在」，
//  也拿不到「旧的路径」去通知 MainWindow 同步标签页。
//  这里改成：双击 F2 后只弹一个 QLineEdit 覆盖在文件上，回车时由我们自己调
//  FileOps::rename()，成功后再发 fileRenamed 信号。
// ---------------------------------------------------------------------------
namespace {

class RenameDelegate : public QStyledItemDelegate
{
public:
    // 不用 Q_OBJECT + 自定义信号，改成回调：
    // 这样 .cpp 里的类不需要 moc，也就不必在文件尾部 #include "xxx.moc"
    std::function<void(const QString &)> onCommit;

    explicit RenameDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

protected:
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &) const override
    {
        auto *ed = new QLineEdit(parent);
        ed->setFrame(false);
        ed->setObjectName(QStringLiteral("renameEditor"));
        // 编辑器一关（回车 / 点别处）就把「新名字」交回去，由 FileExplorer 决定是否改名
        QObject::connect(ed, &QLineEdit::editingFinished, ed, [this, ed] {
            if (onCommit)
                onCommit(ed->text());
        });
        return ed;
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        auto *ed = qobject_cast<QLineEdit *>(editor);
        if (!ed)
            return;
        const QString name = index.data(Qt::EditRole).toString();
        ed->setText(name);
        // 只选中主干，扩展名保持不选中（和系统资源管理器一致）
        ed->setSelection(0, FileOps::baseNameLength(name));
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    {
        editor->setGeometry(option.rect);
    }
};

} // namespace

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

    // ---- 右键菜单
    //  在 QTreeView 上开 CustomContextMenu，右键时会先「选中」被点的项再弹菜单，
    //  这样「右键空白处新建」「右键某项改名」可以统一按「当前选中项」处理。
    _tree->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(_tree, &QTreeView::customContextMenuRequested,
                     this, [this](const QPoint &pos) {
        showContextMenu(_tree->viewport()->mapToGlobal(pos));
    });
    // 右键时不要弹两次（QTreeView 默认不会自己弹，这里只是确保没有默认菜单）
    _tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _tree->setAlternatingRowColors(false);
    _tree->setDragEnabled(false);
    _tree->setAcceptDrops(false);

    // ---- 重命名：用自定义委托，回车后自己处理改名
    auto *delegate = new RenameDelegate(_tree);
    delegate->onCommit = [this](const QString &name) { onCommitRename(name); };
    _tree->setItemDelegate(delegate);
    _tree->setEditTriggers(QAbstractItemView::NoEditTriggers);   // 只在 F2/菜单时进入编辑

    // ---- F2 / Delete
    auto *renameAct = new QAction(this);
    renameAct->setShortcut(QKeySequence(Qt::Key_F2));
    renameAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(renameAct, &QAction::triggered, this, &FileExplorer::renameCurrent);
    _tree->addAction(renameAct);

    auto *delAct = new QAction(this);
    delAct->setShortcuts({QKeySequence(Qt::Key_Delete)});
    delAct->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(delAct, &QAction::triggered, this, &FileExplorer::doDelete);
    _tree->addAction(delAct);
    _tree->setFocusPolicy(Qt::StrongFocus);

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

// ---------------------------------------------------------------------------
//  选中 / 定位
// ---------------------------------------------------------------------------
QStringList FileExplorer::selectedPaths() const
{
    QStringList out;
    const auto rows = _tree->selectionModel()->selectedRows(0);
    for (const QModelIndex &idx : rows) {
        const QString p = _model->filePath(idx);
        if (!p.isEmpty())
            out << p;
    }
    // 没有选中但有 currentIndex 时，也把它算上（右键菜单依赖）
    if (out.isEmpty() && _tree->currentIndex().isValid()) {
        const QString p = _model->filePath(_tree->currentIndex());
        if (!p.isEmpty())
            out << p;
    }
    return out;
}

void FileExplorer::revealPath(const QString &path)
{
    if (path.isEmpty())
        return;
    const QModelIndex idx = _model->index(path);
    if (!idx.isValid())
        return;

    // 逐级展开父节点，否则 index 虽然有效但不可见
    QModelIndex parent = idx.parent();
    while (parent.isValid()) {
        _tree->setExpanded(parent, true);
        parent = parent.parent();
    }
    _tree->setCurrentIndex(idx);
    _tree->scrollTo(idx);
}

// ---------------------------------------------------------------------------
//  新建时的落点目录
//   选中文件夹       -> 该文件夹内部
//   选中文件         -> 它的父文件夹
//   什么都没选        -> 根目录
// ---------------------------------------------------------------------------
QString FileExplorer::targetDirForNew() const
{
    const QStringList sel = selectedPaths();
    if (sel.isEmpty())
        return _rootPath;

    const QFileInfo fi(sel.first());
    if (fi.isDir())
        return fi.absoluteFilePath();
    return fi.absolutePath();
}

// ---------------------------------------------------------------------------
//  右键菜单
// ---------------------------------------------------------------------------
void FileExplorer::showContextMenu(const QPoint &globalPos)
{
    const QModelIndex hit = _tree->indexAt(_tree->viewport()->mapFromGlobal(globalPos));

    // 右键空白处：不改变已有选中，但仍以「根目录」为落点
    if (hit.isValid() && !_tree->selectionModel()->isSelected(hit)) {
        _tree->setCurrentIndex(hit);           // 顺手把被右键的那项设成当前项
    }

    const QStringList sel      = selectedPaths();
    const bool hasSel          = !sel.isEmpty();
    const QFileInfo selInfo    = hasSel ? QFileInfo(sel.first()) : QFileInfo();
    const bool singleSel       = (sel.size() == 1);
    const bool onFile          = hasSel && selInfo.isFile();
    const bool onDir           = hasSel && selInfo.isDir();

    QMenu menu(this);

    QAction *actNewFile   = menu.addAction(QStringLiteral("新建文件…"));
    QAction *actNewFolder = menu.addAction(QStringLiteral("新建文件夹…"));
    menu.addSeparator();

    QAction *actOpen      = menu.addAction(QStringLiteral("打开"));
    menu.addSeparator();

    QAction *actRename    = menu.addAction(QStringLiteral("重命名\tF2"));
    QAction *actDelete    = menu.addAction(QStringLiteral("删除\tDelete"));
    menu.addSeparator();

    QAction *actReveal    = menu.addAction(QStringLiteral("在资源管理器中显示"));
    QAction *actProps     = menu.addAction(QStringLiteral("属性…"));
    menu.addSeparator();
    QAction *actRefresh   = menu.addAction(QStringLiteral("刷新"));

    // 可用性
    actOpen->setEnabled(singleSel && onFile);
    actRename->setEnabled(singleSel);          // 只允许单个重命名
    actDelete->setEnabled(hasSel);
    actReveal->setEnabled(hasSel);
    actProps->setEnabled(hasSel);
    // 不允许把根目录本身改名/删掉（会把整个工作区搞乱）
    if (singleSel && QDir(selInfo.absoluteFilePath()) == QDir(_rootPath)) {
        actRename->setEnabled(false);
        actDelete->setEnabled(false);
    }

    QAction *chosen = menu.exec(globalPos);
    if (!chosen)
        return;

    if (chosen == actNewFile)        doNewFile();
    else if (chosen == actNewFolder) doNewFolder();
    else if (chosen == actOpen)      emit fileActivated(sel.first());
    else if (chosen == actRename)    renameCurrent();
    else if (chosen == actDelete)    doDelete();
    else if (chosen == actReveal)    FileOps::revealInExplorer(this, sel.first());
    else if (chosen == actProps)     doProperties();
    else if (chosen == actRefresh) {
        const QString r = _rootPath;
        _model->setRootPath(QString());        // 触发一次重新扫描
        _model->setRootPath(r);
    }
}

// ---------------------------------------------------------------------------
//  新建文件 / 文件夹
// ---------------------------------------------------------------------------
void FileExplorer::doNewFile()
{
    const QString dir = targetDirForNew();
    const QString created = FileOps::newFile(this, dir);
    if (created.isEmpty())
        return;

    // 新建后展开所在目录并定位
    _tree->setExpanded(_model->index(dir), true);
    revealPath(created);
    // 新建出来的空文件直接帮用户打开，符合直觉
    emit fileActivated(created);
}

void FileExplorer::doNewFolder()
{
    const QString dir = targetDirForNew();
    const QString created = FileOps::newFolder(this, dir);
    if (created.isEmpty())
        return;

    _tree->setExpanded(_model->index(dir), true);
    revealPath(created);
}

// ---------------------------------------------------------------------------
//  删除
// ---------------------------------------------------------------------------
void FileExplorer::doDelete()
{
    const QStringList sel = selectedPaths();
    if (sel.isEmpty())
        return;

    // 根目录保护
    if (QDir(sel.first()) == QDir(_rootPath))
        return;

    if (!FileOps::removeToTrash(this, sel))
        return;                                  // 用户取消或失败

    for (const QString &p : sel) {
        // 关掉仍然指向这个路径的标签页（MainWindow 里处理）
        emit pathDeleted(p);
    }
}

// ---------------------------------------------------------------------------
//  属性
// ---------------------------------------------------------------------------
void FileExplorer::doProperties()
{
    const QStringList sel = selectedPaths();
    if (sel.isEmpty())
        return;
    FileOps::showProperties(this, sel.first());
}

// ---------------------------------------------------------------------------
//  重命名
//
//  走 QTreeView 的 edit()，用 RenameDelegate 弹 QLineEdit；
//  编辑结束时 closeEditor -> onCommitRename 统一提交。
// ---------------------------------------------------------------------------
void FileExplorer::renameCurrent()
{
    const QStringList sel = selectedPaths();
    if (sel.size() != 1)
        return;
    if (QDir(sel.first()) == QDir(_rootPath))
        return;                                  // 根目录不允许改名

    const QModelIndex idx = _model->index(sel.first());
    if (!idx.isValid())
        return;

    _renamingPath = sel.first();
    _tree->setCurrentIndex(idx);
    _tree->edit(idx);                            // 委托接管，弹出 QLineEdit
}

void FileExplorer::onCommitRename(const QString &newName)
{
    if (_renamingPath.isEmpty())
        return;

    const QString oldPath = _renamingPath;
    _renamingPath.clear();

    QString err;
    const QString newPath = FileOps::rename(this, oldPath, newName, &err);
    if (newPath.isEmpty()) {
        if (!err.isEmpty())
            QMessageBox::warning(this, QStringLiteral("重命名"), err);
        return;                                  // 失败：保持原状
    }

    emit fileRenamed(oldPath, newPath);

    // 让树立刻反映新名字（模型有缓存，靠 setRootPath 刷一下更保险）
    _tree->setExpanded(_model->index(QFileInfo(newPath).absolutePath()), true);
    revealPath(newPath);
}
