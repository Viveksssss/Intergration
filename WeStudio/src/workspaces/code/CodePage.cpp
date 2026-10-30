#include "CodePage.h"

#include "CodeEditor.h"
#include "CodeTheme.h"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QStatusBar>
#include <QTextStream>
#include <QToolBar>

namespace {

// 演示用的示例代码。
// 特意包含了：多行注释 / 单行注释 / 字符串 / 嵌套 {} / 一行里的 {} ——
// 这几样正好覆盖高亮和折叠最容易出错的场景。
const char *kDemoSource = R"SRC(// ------------------------------------------------------------------
//  演示文件：这块内容会被 CodeHighlighter 着色、被 FoldManager 扫描
//  点行号左边的三角可以折叠；光标贴着括号会高亮配对的另一个
// ------------------------------------------------------------------
#include <QApplication>
#include <QPlainTextEdit>

/* 这是一个多行注释，
   它会跨越好几行。
   它存在的意义是测试 QSyntaxHighlighter 的
   previousBlockState / setCurrentBlockState 有没有传对。 */

namespace demo {

class Shape
{
public:
    explicit Shape(int sides) : m_sides(sides) {}
    virtual ~Shape() = default;

    virtual double area() const { return 0.0; }
    int sides() const { return m_sides; }

private:
    int m_sides = 4;
};

double Shape::area() const
{
    // 单行注释：下面的数字和字符串都不应该被当成关键字
    const double pi = 3.1415926;
    const char *name = "a string with { braces } inside";

    if (m_sides > 100) {
        return pi * 100.0;
    } else {
        for (int i = 0; i < m_sides; ++i) {
            if (i % 2 == 0) {
                continue;
            }
        }
    }
    return 0.0;
}

}  // namespace demo

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Shape s(8);
    QPlainTextEdit editor;
    editor.setPlainText("hello { world }");
    editor.show();

    return app.exec();
}
)SRC";

}  // namespace

CodePage::CodePage(QWidget *parent)
    : QMainWindow(parent)
{
    m_editor = new CodeEditor(this);
    setCentralWidget(m_editor);

    buildToolBar();
    buildStatusBar();

    connect(m_editor, &CodeEditor::cursorMoved, this, &CodePage::updateStatus);
    connect(m_editor, &CodeEditor::foldStateChanged, this, &CodePage::updateFoldActions);

    loadDemoDocument();
    resize(1080, 720);
}

void CodePage::buildToolBar()
{
    QToolBar *bar = addToolBar(QStringLiteral("主工具栏"));
    bar->setMovable(false);
    bar->setIconSize(QSize(16, 16));

    QAction *openAct = bar->addAction(QStringLiteral("打开文件"));
    connect(openAct, &QAction::triggered, this, &CodePage::openFile);

    bar->addSeparator();

    QAction *gotoAct = bar->addAction(QStringLiteral("跳转行"));
    connect(gotoAct, &QAction::triggered, this, &CodePage::jumpToLine);

    bar->addSeparator();

    m_foldAllAct = bar->addAction(QStringLiteral("全部折叠"));
    connect(m_foldAllAct, &QAction::triggered, m_editor, &CodeEditor::foldAll);

    m_unfoldAllAct = bar->addAction(QStringLiteral("全部展开"));
    connect(m_unfoldAllAct, &QAction::triggered, m_editor, &CodeEditor::unfoldAll);

    bar->addSeparator();

    QAction *wrapAct = bar->addAction(QStringLiteral("自动换行"));
    wrapAct->setCheckable(true);
    wrapAct->setChecked(false);
    connect(wrapAct, &QAction::toggled, m_editor, &CodeEditor::setWordWrapEnabled);
}

void CodePage::buildStatusBar()
{
    m_posLabel  = new QLabel(this);
    m_statLabel = new QLabel(this);

    statusBar()->addWidget(m_posLabel);
    statusBar()->addPermanentWidget(m_statLabel);
    statusBar()->setSizeGripEnabled(false);
}

void CodePage::loadDemoDocument()
{
    m_editor->setPlainText(QString::fromUtf8(kDemoSource));
    m_editor->document()->setModified(false);
    setWindowTitle(QStringLiteral("CodeEditor — 演示文件"));

    updateStatus(1, 1);
    updateFoldActions();
    m_editor->setFocus();
}

void CodePage::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开文件"), QString(),
        QStringLiteral("源代码 (*.cpp *.h *.hpp *.c *.py *.js *.qml *.txt);;所有文件 (*)"));

    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage(QStringLiteral("打开失败：%1").arg(path), 4000);
        return;
    }

    QTextStream in(&file);
    // 简单起见不做编码探测；真实项目里应该用 QTextCodec / uchardet
    in.setEncoding(QStringConverter::Utf8);
    m_editor->setPlainText(in.readAll());
    m_editor->document()->setModified(false);

    setWindowTitle(QStringLiteral("CodeEditor — %1").arg(QFileInfo(path).fileName()));
    updateFoldActions();
}

void CodePage::jumpToLine()
{
    bool ok = false;
    const int line = QInputDialog::getInt(
        this, QStringLiteral("跳转行"),
        QStringLiteral("行号（1 - %1）：").arg(m_editor->blockCount()),
        1, 1, qMax(1, m_editor->blockCount()), 1, &ok);

    if (ok)
        m_editor->gotoLine(line);
}

void CodePage::updateStatus(int line, int column)
{
    m_posLabel->setText(QStringLiteral("  行 %1, 列 %2").arg(line).arg(column));
    m_statLabel->setText(m_editor->statsText() + QStringLiteral("   "));
}

void CodePage::updateFoldActions()
{
    // 折叠区数量为 0 时，按钮禁掉 —— 顺便让用户知道「这个文件没东西可折」
    const bool hasFold = m_editor->document() && m_editor->blockCount() > 0;
    if (m_foldAllAct)
        m_foldAllAct->setEnabled(hasFold);
}
