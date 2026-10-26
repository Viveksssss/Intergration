#include "MainWindow.h"

#include "CodeEditor.h"
#include "Theme.h"
#include <QMessageBox>
#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    _editor = new CodeEditor(this);
    setCentralWidget(_editor);

    buildToolBar();
    buildStatusBar();

    connect(_editor, &CodeEditor::cursorMoved, this, &MainWindow::updateStatus);
    connect(_editor, &CodeEditor::foldStateChanged, this, &MainWindow::updateFoldActions);

    // loadDemoDocument();
    resize(1080, 720);
}

MainWindow::~MainWindow()
{
    if (_editor->document()->isModified()) {
        const auto ret = QMessageBox::question(
            this,
            QStringLiteral("确认"),
            QStringLiteral("文件已修改，是否保存？"),
            QMessageBox::Yes | QMessageBox::No ,
            QMessageBox::Yes);   // 默认按钮
        if (ret == QMessageBox::Yes) {
            saveFile();
        }
    }


}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开文件"), QString(),
        QStringLiteral("所有文件 (*);;源代码 (*.cpp *.h *.hpp *.c *.py *.js *.qml *.txt)"));

    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage(QStringLiteral("打开失败：%1").arg(path), 4000);
        return;
    }

    _currentPath = path;
    QCoreApplication::setApplicationName(QFileInfo(path).fileName());
    QTextStream in(&file);
    // 简单起见不做编码探测；真实项目里应该用 QTextCodec / uchardet
    in.setEncoding(QStringConverter::Utf8);
    _editor->setPlainText(in.readAll());
    _editor->document()->setModified(false);

    setWindowTitle(QStringLiteral("CodeEditor — %1").arg(QFileInfo(path).fileName()));
    updateFoldActions();
    _openFromFile = true;
}

void MainWindow::jumpToLine()
{
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
    _posLabel->setText(QStringLiteral("  行 %1, 列 %2").arg(line).arg(column));
    _statLabel->setText(_editor->statsText() + QStringLiteral("   "));
}

void MainWindow::updateFoldActions()
{
    const bool hasFold = _editor->document() && _editor->blockCount() > 0;
    if (_foldAllAct)
        _foldAllAct->setEnabled(hasFold);
}

void MainWindow::saveFile()
{
    if (_currentPath.isEmpty()) {
        // 没打开过文件 → 另存为
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存文件"), QString(),
            QStringLiteral("所有文件 (*)"));
        if (path.isEmpty())
            return;
        _currentPath = path;
    }

    QFile file(_currentPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage(QStringLiteral("保存失败：%1").arg(_currentPath), 4000);
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << _editor->toPlainText();
    file.close();

    _editor->document()->setModified(false);
    setWindowTitle(QStringLiteral("CodeEditor — %1").arg(QFileInfo(_currentPath).fileName()));
    statusBar()->showMessage(QStringLiteral("已保存"), 2000);
}

void MainWindow::buildToolBar()
{
    QToolBar *bar = addToolBar(QStringLiteral("主工具栏"));
    bar->setMovable(false);
    bar->setIconSize(QSize(16, 16));

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
    connect(_foldAllAct, &QAction::triggered, _editor, &CodeEditor::foldAll);

    _unfoldAllAct = bar->addAction(QStringLiteral("全部展开"));
    _unfoldAllAct->setToolTip("全部展开\tCtrl+Shift+]");
    _unfoldAllAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+]")));
    connect(_unfoldAllAct, &QAction::triggered, _editor, &CodeEditor::unfoldAll);

    bar->addSeparator();

    QAction *wrapAct = bar->addAction(QStringLiteral("自动换行"));
    wrapAct->setToolTip("自动换行\tAlt+Z");
    wrapAct->setShortcut(QStringLiteral("Alt+Z"));
    wrapAct->setCheckable(true);
    wrapAct->setChecked(false);
    connect(wrapAct, &QAction::toggled, _editor, &CodeEditor::setWordWrapEnabled);
}

void MainWindow::buildStatusBar()
{
    _posLabel  = new QLabel(this);
    _statLabel = new QLabel(this);

    statusBar()->addWidget(_posLabel);
    statusBar()->addPermanentWidget(_statLabel);
    statusBar()->setSizeGripEnabled(false);
}
