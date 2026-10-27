#include "MainWindow.h"

#include "CodeEditor.h"
#include "EditorTabs.h"
#include "FileExplorer.h"
#include "Theme.h"
#include <QMessageBox>
#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextCursor>
#include <QTextStream>
#include <QToolBar>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 中间区域：左资源管理器 + 右标签页（QSplitter 可拖动分隔条改宽度）
    buildCentralArea();

    buildToolBar();
    buildStatusBar();
    buildShortcuts();

    // 启动时没有打开任何文件：_editor 为空，先把状态栏/折叠按钮置成空态
    resetEditorState();

    // loadDemoDocument();
    resize(1180, 760);
}

MainWindow::~MainWindow()
{
}

// ---------------------------------------------------------------------------
//  关窗口：逐个标签页确认未保存修改
//  必须在 closeEvent 里做，不能在析构函数里 —— 析构时再弹模态对话框
//  （尤其是弹另存为文件框）在新窗口树里会出各种异常。
// ---------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!_tabs) {
        QMainWindow::closeEvent(event);
        return;
    }

    // 从后往前遍历：maybeClose 会 removeTab，倒序不会打乱未处理的下标
    for (int i = _tabs->count() - 1; i >= 0; --i) {
        if (!_tabs->editorAt(i))
            continue;                       // 跳过欢迎页
        if (!_tabs->maybeClose(i)) {
            event->ignore();                // 用户点了取消：窗口不关
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------------
//  中间区域 = QSplitter( 左侧 FileExplorer | 右侧 EditorTabs )
// ---------------------------------------------------------------------------
void MainWindow::buildCentralArea()
{
    _explorer = new FileExplorer(this);
    _tabs     = new EditorTabs(this);

    _splitter = new QSplitter(Qt::Horizontal, this);
    _splitter->setObjectName(QStringLiteral("mainSplitter"));
    _splitter->setHandleWidth(1);
    _splitter->setChildrenCollapsible(false);
    _splitter->addWidget(_explorer);
    _splitter->addWidget(_tabs);
    _splitter->setStretchFactor(0, 0);      // 左侧宽度固定，拉伸时给右边
    _splitter->setStretchFactor(1, 1);
    _splitter->setSizes({240, 940});

    setCentralWidget(_splitter);
    _explorer->setVisible(false);           // 没打开文件夹之前先藏起来

    // 资源管理器：点文件 -> 开标签页；点标题栏按钮 -> 选文件夹
    connect(_explorer, &FileExplorer::fileActivated,
            this, &MainWindow::onFileActivated);
    connect(_explorer, &FileExplorer::openFolderRequested,
            this, &MainWindow::openFolder);

    // 标签页切换 -> 换当前编辑器
    connect(_tabs, &EditorTabs::currentEditorChanged,
            this, &MainWindow::onEditorChanged);

    // 点空白页也能直接开文件夹，符合「空窗口就该干这个」的直觉
    connect(_tabs, &EditorTabs::blankPageClicked,
            this, &MainWindow::openFolder);
}
void MainWindow::openPath(const QString &path)
{
    onFileActivated(path);
}

void MainWindow::openContent(const QString &title, const QString &text)
{
    _tabs->openContent(title, text, QString());
}

// ---------------------------------------------------------------------------
//  资源管理器 -> 打开文件
// ---------------------------------------------------------------------------
void MainWindow::onFileActivated(const QString &path)
{
    if (!_tabs->openFile(path)) {
        statusBar()->showMessage(QStringLiteral("打开失败：%1").arg(path), 4000);
        return;
    }
    // 打开后顺手把上级文件夹设成根（只有第一次 / 跨目录时才换，避免树一直跳）
    if (_explorer->rootPath().isEmpty() || !QDir(_explorer->rootPath()).exists()) {
        _explorer->openFolder(QFileInfo(path).absolutePath());
        _explorer->setVisible(true);
    }
}

void MainWindow::openFolder()
{
    // 打开文件夹对话框：默认从当前文件夹 / 上一级开始
    const QString start = _explorer->rootPath().isEmpty()
                              ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                              : _explorer->rootPath();
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("打开文件夹"), start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dir.isEmpty())
        return;

    if (!_explorer->openFolder(dir)) {
        statusBar()->showMessage(QStringLiteral("无法打开文件夹：%1").arg(dir), 4000);
        return;
    }

    _explorer->setVisible(true);
    _explorerAct->setChecked(true);
    statusBar()->showMessage(QStringLiteral("已打开文件夹：%1").arg(QDir::toNativeSeparators(dir)), 3000);
}

void MainWindow::toggleExplorer()
{
    if (_explorer->rootPath().isEmpty()) {
        // 还没选过文件夹：直接把打开文件夹的动作做掉
        _explorerAct->setChecked(false);
        openFolder();
        return;
    }
    _explorer->setVisible(_explorerAct->isChecked());
}

// ---------------------------------------------------------------------------
//  当前编辑器切换：原来的所有 connect 都重新绑到新编辑器上
// ---------------------------------------------------------------------------
void MainWindow::onEditorChanged(CodeEditor *editor)
{
    attachEditor(editor);
}

void MainWindow::attachEditor(CodeEditor *editor)
{
    _editor = editor;

    if (_connected && _connected != editor) {
        disconnect(_connected, &CodeEditor::cursorMoved,      this, &MainWindow::updateStatus);
        disconnect(_connected, &CodeEditor::foldStateChanged, this, &MainWindow::updateFoldActions);
    }
    if (editor && _connected != editor) {
        connect(editor, &CodeEditor::cursorMoved,      this, &MainWindow::updateStatus);
        connect(editor, &CodeEditor::foldStateChanged, this, &MainWindow::updateFoldActions);
    }
    _connected = editor;

    if (!editor) {
        resetEditorState();
        return;
    }

    // 标题：CodeEditor — 文件名
    const int idx = _tabs->currentIndex();
    const QString path = _tabs->filePathOf(idx);
    setWindowTitle(path.isEmpty()
                       ? QStringLiteral("CodeEditor — 未命名")
                       : QStringLiteral("CodeEditor — %1").arg(QFileInfo(path).fileName()));

    updateFoldActions();
    editor->setFocus();

    // 同步状态栏的行/列（切标签时光标位置不一定会触发信号）
    const QTextCursor c = editor->textCursor();
    updateStatus(c.blockNumber() + 1, c.positionInBlock() + 1);

    // 每个编辑器都是新 document，原来的 _openFromFile / 折叠状态都要刷一次
    _openFromFile = !path.isEmpty();
}

void MainWindow::resetEditorState()
{
    _editor = nullptr;
    _connected = nullptr;
    setWindowTitle(QStringLiteral("CodeEditor"));
    if (_posLabel)
        _posLabel->setText(QStringLiteral("  行 -, 列 -"));
    if (_statLabel)
        _statLabel->setText(QStringLiteral("0 行  0 字符   "));
    if (_foldAllAct)
        _foldAllAct->setEnabled(false);
    if (_unfoldAllAct)
        _unfoldAllAct->setEnabled(false);
    _openFromFile = false;
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开文件"),
        _explorer->rootPath().isEmpty() ? QString() : _explorer->rootPath(),
        QStringLiteral("所有文件 (*);;源代码 (*.cpp *.h *.hpp *.c *.py *.js *.qml *.txt)"));

    if (path.isEmpty())
        return;

    onFileActivated(path);
}

void MainWindow::jumpToLine()
{
    if (!_editor)
        return;

    bool ok = false;
    const int line = QInputDialog::getInt(
        this, QStringLiteral("跳转行"),
        QStringLiteral("行号（1 - %1）：").arg(_editor->blockCount()),
        1, 1, qMax(1, _editor->blockCount()), 1, &ok);

    if (ok)
        _editor->gotoline(line);
}

void MainWindow::updateStatus(int line, int column)
{
    if (!_posLabel || !_statLabel)
        return;
    if (!_editor) {
        resetEditorState();
        return;
    }
    _posLabel->setText(QStringLiteral("  行 %1, 列 %2").arg(line).arg(column));
    _statLabel->setText(_editor->statsText() + QStringLiteral("   "));
}

void MainWindow::updateFoldActions()
{
    const bool hasFold = _editor && _editor->document() && _editor->blockCount() > 0;
    if (_foldAllAct)
        _foldAllAct->setEnabled(hasFold);
    if (_unfoldAllAct)
        _unfoldAllAct->setEnabled(hasFold);
}

void MainWindow::saveFile()
{
    if (!_editor) {
        statusBar()->showMessage(QStringLiteral("没有打开的文件"), 2000);
        return;
    }

    const int idx = _tabs->currentIndex();

    // 没打开过文件 -> 另存为（和原来的逻辑一致，只是下沉到 EditorTabs 里做 IO）
    if (_tabs->filePathOf(idx).isEmpty()) {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存文件"), QString(),
            QStringLiteral("所有文件 (*)"));
        if (path.isEmpty())
            return;
        if (!_tabs->saveIndex(idx, path)) {
            statusBar()->showMessage(QStringLiteral("保存失败：%1").arg(path), 4000);
            return;
        }
    } else if (!_tabs->saveIndex(idx)) {
        statusBar()->showMessage(QStringLiteral("保存失败：%1").arg(_tabs->filePathOf(idx)), 4000);
        return;
    }

    setWindowTitle(QStringLiteral("CodeEditor — %1")
                       .arg(QFileInfo(_tabs->filePathOf(idx)).fileName()));
    statusBar()->showMessage(QStringLiteral("已保存"), 2000);
}

void MainWindow::buildToolBar()
{
    QToolBar *bar = addToolBar(QStringLiteral("主工具栏"));
    bar->setMovable(false);
    bar->setIconSize(QSize(16, 16));

    // ── 新增：打开文件夹（放在最前面，因为它是资源管理器的入口）
    QAction *openDirAct = bar->addAction(QStringLiteral("打开文件夹"));
    openDirAct->setToolTip(QStringLiteral("打开文件夹\tCtrl+Shift+O"));
    openDirAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(openDirAct, &QAction::triggered, this, &MainWindow::openFolder);

    _explorerAct = bar->addAction(QStringLiteral("资源管理器"));
    _explorerAct->setToolTip(QStringLiteral("显示/隐藏左侧资源管理器\tCtrl+B"));
    _explorerAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
    _explorerAct->setCheckable(true);
    _explorerAct->setChecked(false);
    connect(_explorerAct, &QAction::toggled, this, &MainWindow::toggleExplorer);

    bar->addSeparator();

    QAction *openAct = bar->addAction(QStringLiteral("打开文件"));
    openAct->setToolTip("打开文件Ctrl+O");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);

    QAction *saveAct = bar->addAction(QStringLiteral("保存"));   // 或 "保存"、其他符号
    saveAct->setShortcut(QKeySequence::Save);                 // Ctrl+S（跨平台标准）
    saveAct->setToolTip(QStringLiteral("保存Ctrl+S"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::saveFile);

    bar->addSeparator();

    QAction *gotoAct = bar->addAction(QStringLiteral("跳转行"));
    gotoAct->setToolTip("跳转Ctrl+G");
    gotoAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));   // 无标准枚举，手写
    connect(gotoAct, &QAction::triggered, this, &MainWindow::jumpToLine);

    bar->addSeparator();

    _foldAllAct = bar->addAction(QStringLiteral("全部折叠"));
    _foldAllAct->setToolTip("全部折叠\tCtrl+Shift+[");
    _foldAllAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+[")));
    // 注意：不能直接 connect 到 _editor（切换标签会变），统一转发到「当前编辑器」
    connect(_foldAllAct, &QAction::triggered, this, [this] {
        if (_editor) _editor->foldAll();
    });

    _unfoldAllAct = bar->addAction(QStringLiteral("全部展开"));
    _unfoldAllAct->setToolTip("全部展开\tCtrl+Shift+]");
    _unfoldAllAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+]")));
    connect(_unfoldAllAct, &QAction::triggered, this, [this] {
        if (_editor) _editor->unfoldAll();
    });

    bar->addSeparator();

    // ── 关闭相关：之前只有标签上的小 ×，容易找不到，这里补一个显式按钮
    QAction *closeAct = bar->addAction(QStringLiteral("关闭标签"));
    closeAct->setToolTip(QStringLiteral("关闭当前标签\tCtrl+W"));
    closeAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    connect(closeAct, &QAction::triggered, _tabs, &EditorTabs::closeCurrent);

    QAction *closeAllAct = bar->addAction(QStringLiteral("全部关闭"));
    closeAllAct->setToolTip(QStringLiteral("关闭所有标签\tCtrl+Shift+W"));
    connect(closeAllAct, &QAction::triggered, _tabs, &EditorTabs::closeAll);

    bar->addSeparator();

    QAction *wrapAct = bar->addAction(QStringLiteral("自动换行"));
    wrapAct->setToolTip("自动换行\tAlt+Z");
    wrapAct->setShortcut(QStringLiteral("Alt+Z"));
    wrapAct->setCheckable(true);
    wrapAct->setChecked(false);
    connect(wrapAct, &QAction::toggled, this, [this](bool on) {
        // 换行开关对所有标签统一生效，不只是当前那个
        for (int i = 0; i < _tabs->count(); ++i)
            if (CodeEditor *e = _tabs->editorAt(i))
                e->setWordWrapEnabled(on);
    });
}

void MainWindow::buildShortcuts()
{
    // 关标签的快捷键挂在窗口上（WindowShortcut 作用域）：
    // 焦点在编辑器里时也能触发，而且不会和 QPlainTextEdit 抢 Ctrl+W。
    auto bind = [this](const QKeySequence &key, void (EditorTabs::*fn)()) {
        auto *act = new QAction(this);
        act->setShortcut(key);
        act->setShortcutContext(Qt::WindowShortcut);
        connect(act, &QAction::triggered, _tabs, fn);
        addAction(act);
    };

    bind(QKeySequence(QStringLiteral("Ctrl+W")),        &EditorTabs::closeCurrent);
    bind(QKeySequence(QStringLiteral("Ctrl+F4")),       &EditorTabs::closeCurrent);
    bind(QKeySequence(QStringLiteral("Ctrl+Shift+W")),  &EditorTabs::closeAll);
}

void MainWindow::buildStatusBar()
{
    _posLabel  = new QLabel(this);
    _statLabel = new QLabel(this);

    statusBar()->addWidget(_posLabel);
    statusBar()->addPermanentWidget(_statLabel);
    statusBar()->setSizeGripEnabled(false);
}
