#include "editortabs.h"
#include "codeeditor.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QTabBar>
#include <QTextDocument>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// 每个 CodeEditor 上挂的属性名
constexpr auto kPropPath      = "filePath";     // QString：文件绝对路径
constexpr auto kPropSavedText = "savedText";    // QString：上次保存（或打开）时的全文

} // namespace

// ---------------------------------------------------------------------------
//  自绘关闭按钮
//
//  为什么不用 QTabBar 自带的 close-button？
//  因为自带的按钮完全由 QStyle + QSS 的 ::close-button 决定：
//  本项目的 theme.cpp 里没有给它设置 image，Fusion 风格下描边颜色又极暗
//  （≈#2c2c36，而标签底色也是 #1a1a21），肉眼几乎看不见，
//  再叠上 `image:none` 就彻底成一个「看不见但能点」的空洞。
//  自己画一个 × 最省事，也不受任何样式表影响。
// ---------------------------------------------------------------------------
namespace {

constexpr int kCloseBtnSize = 14;
constexpr auto kPropIndex   = "tabIndex";

class CloseButton : public QToolButton
{
public:
    explicit CloseButton(int tabIndex, QWidget *parent = nullptr)
        : QToolButton(parent), _tabIndex(tabIndex)
    {
        setCursor(Qt::ArrowCursor);
        setFixedSize(kCloseBtnSize, kCloseBtnSize);
        setFocusPolicy(Qt::NoFocus);          // 别抢走编辑器的键盘焦点
        setToolTip(QStringLiteral("关闭 (Ctrl+W)"));
        setAttribute(Qt::WA_Hover, true);
        setProperty(kPropIndex, tabIndex);

        connect(this, &QToolButton::clicked, this, [this] {
            // 点击时把「关闭第几个」的请求转给 tabBar，由 EditorTabs 统一处理
            if (auto *bar = qobject_cast<QTabBar *>(parentWidget()))
                emit bar->tabCloseRequested(_tabIndex);
        });
    }

    void setTabIndex(int i)
    {
        _tabIndex = i;
        setProperty(kPropIndex, i);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const bool hot = underMouse();
        if (hot) {
            p.setPen(Qt::NoPen);
            p.setBrush(Theme::kSelectionBg);      // 悬停：蓝底
            p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 3, 3);
        }

        // 一根 ×：两条 45° 线
        p.setPen(QPen(hot ? QColor(0xff, 0xff, 0xff) : QColor(0x9a, 0x9a, 0xac), 1.4,
                      Qt::SolidLine, Qt::RoundCap));
        const int m = 4;
        p.drawLine(m, m, width() - m - 1, height() - m - 1);
        p.drawLine(width() - m - 1, m, m, height() - m - 1);
    }

private:
    int _tabIndex = -1;
};

} // namespace

// ---------------------------------------------------------------------------
//  空态占位页：中间一段提示文字
// ---------------------------------------------------------------------------
namespace {
class BlankPage : public QWidget
{
public:
    explicit BlankPage(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("blankPage"));
        setAttribute(Qt::WA_StyledBackground, true);

        auto *lay = new QVBoxLayout(this);
        lay->setAlignment(Qt::AlignCenter);
        lay->setSpacing(10);

        auto *title = new QLabel(QStringLiteral("CodeEditor"), this);
        title->setObjectName(QStringLiteral("blankTitle"));
        title->setAlignment(Qt::AlignCenter);

        auto *hint = new QLabel(QStringLiteral("未打开文件 — 用左上角「打开文件夹」浏览项目，"
                                               "或在工具栏「打开文件」(Ctrl+O)"),
                                this);
        hint->setObjectName(QStringLiteral("blankHint"));
        hint->setAlignment(Qt::AlignCenter);

        lay->addStretch(1);
        lay->addWidget(title);
        lay->addWidget(hint);
        lay->addStretch(1);
    }
};
} // namespace

// ---------------------------------------------------------------------------
EditorTabs::EditorTabs(QWidget *parent)
    : QTabWidget(parent)
{
    setObjectName(QStringLiteral("editorTabs"));
    setDocumentMode(true);
    setTabsClosable(true);
    setMovable(true);
    setElideMode(Qt::ElideRight);
    setUsesScrollButtons(true);
    setTabBarAutoHide(false);

    _blank = new BlankPage(this);
    addTab(_blank, QStringLiteral("欢迎"));

    // 欢迎页不可关闭
    if (QTabBar *bar = tabBar()) {
        bar->setTabButton(0, QTabBar::RightSide, nullptr);
        bar->setTabButton(0, QTabBar::LeftSide, nullptr);
        bar->setDrawBase(false);
        bar->setContextMenuPolicy(Qt::CustomContextMenu);
    }

    connect(this, &QTabWidget::currentChanged,
            this, &EditorTabs::onCurrentChanged);
    connect(this, &QTabWidget::tabCloseRequested,
            this, &EditorTabs::onCloseRequested);

    // 标签上右键：关闭 / 关闭其他 / 关闭全部 / 关闭右侧
    if (QTabBar *bar = tabBar()) {
        connect(bar, &QTabBar::customContextMenuRequested,
                this, &EditorTabs::showTabMenu);
    }

    // 点击空白页 -> 外壳可以弹「打开文件夹」
    _blank->installEventFilter(this);
}

// ---------------------------------------------------------------------------
//  打开 / 新建
// ---------------------------------------------------------------------------
CodeEditor *EditorTabs::openFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile())
        return nullptr;

    const QString abs = info.absoluteFilePath();

    // 已经开过了 -> 直接切过去（不做重复打开）
    const int exist = indexOfFile(abs);
    if (exist >= 0) {
        setCurrentIndex(exist);
        if (CodeEditor *e = editorAt(exist))
            e->setFocus();
        return editorAt(exist);
    }

    QFile file(abs);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return nullptr;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);   // 与原 openFile() 保持一致
    const QString text = in.readAll();
    file.close();

    return openContent(info.fileName(), text, abs);
}

CodeEditor *EditorTabs::openContent(const QString &title, const QString &text,
                                    const QString &filePath)
{
    CodeEditor *editor = new CodeEditor(this);
    editor->setProperty(kPropPath, filePath);
    editor->setProperty(kPropSavedText, text);

    // 这里是「整篇替换」，用 beginEditBlock 包一下：
    // 语法高亮和折叠只会触发一次扫描，打开大文件不会卡
    QTextCursor c(editor->document());
    c.beginEditBlock();
    c.insertText(text);
    c.endEditBlock();
    c.movePosition(QTextCursor::Start);
    editor->setTextCursor(c);
    editor->document()->setModified(false);

    connectEditor(editor);

    const QString tabName = title.isEmpty() ? QStringLiteral("未命名") : title;
    const int idx = addTab(editor, tabName);
    setTabToolTip(idx, filePath.isEmpty() ? tabName : QDir::toNativeSeparators(filePath));
    renumberCloseButtons();
    setCurrentIndex(idx);
    editor->setFocus();

    return editor;
}

// ---------------------------------------------------------------------------
//  给每个可关闭的标签装上自绘 ×
//  增删 / 移动标签后，下标会变，所以要整体重挂一遍
// ---------------------------------------------------------------------------
void EditorTabs::renumberCloseButtons()
{
    QTabBar *bar = tabBar();
    if (!bar)
        return;

    for (int i = 0; i < count(); ++i) {
        if (widget(i) == _blank) {
            bar->setTabButton(i, QTabBar::RightSide, nullptr);
            continue;
        }
        // setTabButton 会接管所有权，旧按钮随之销毁，直接新建即可
        auto *btn = new CloseButton(i, bar);
        bar->setTabButton(i, QTabBar::RightSide, btn);
    }
}

// ---------------------------------------------------------------------------
void EditorTabs::connectEditor(CodeEditor *editor)
{
    // 脏标记：文本改没改由我们自己比较（见 isDirty），这里只负责刷新标题。
    // 注意不要用 document()->setModified()，CodeEditor 内部会自己动这个标志。
    QTextDocument *doc = editor->document();
    connect(doc, &QTextDocument::contentsChanged,
            this, [this, editor]() {
        const int idx = indexOfEditor(editor);
        if (idx >= 0)
            refreshTabText(idx);
        emit editorDirtyChanged(editor, isDirty(indexOfEditor(editor)));
    });
}

int EditorTabs::indexOfEditor(CodeEditor *editor) const
{
    for (int i = 0; i < count(); ++i) {
        if (widget(i) == editor)
            return i;
    }
    return -1;
}

int EditorTabs::indexOfFile(const QString &filePath) const
{
    if (filePath.isEmpty())
        return -1;
    const QString abs = QFileInfo(filePath).absoluteFilePath();
    for (int i = 0; i < count(); ++i) {
        if (filePathOf(i) == abs)
            return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
//  查询
// ---------------------------------------------------------------------------
CodeEditor *EditorTabs::currentEditor() const
{
    return qobject_cast<CodeEditor *>(currentWidget());
}

CodeEditor *EditorTabs::editorAt(int index) const
{
    return qobject_cast<CodeEditor *>(widget(index));
}

QString EditorTabs::filePathOf(int index) const
{
    CodeEditor *e = editorAt(index);
    return e ? e->property(kPropPath).toString() : QString();
}

QString EditorTabs::currentFilePath() const
{
    return filePathOf(currentIndex());
}

bool EditorTabs::isDirty(int index) const
{
    CodeEditor *e = editorAt(index);
    if (!e)
        return false;
    return e->toPlainText() != e->property(kPropSavedText).toString();
}

void EditorTabs::refreshTabText(int index)
{
    CodeEditor *e = editorAt(index);
    if (!e)
        return;

    const QString path = e->property(kPropPath).toString();
    QString name = path.isEmpty() ? QStringLiteral("未命名")
                                  : QFileInfo(path).fileName();
    if (name.isEmpty())
        name = QStringLiteral("未命名");
    // 脏了加圆点，干净的不加（比给 Document 设 modified 更直观）
    if (isDirty(index))
        name += QStringLiteral(" ●");

    setTabText(index, name);
}

// ---------------------------------------------------------------------------
//  保存 / 关闭
// ---------------------------------------------------------------------------
bool EditorTabs::saveIndex(int index, const QString &path)
{
    CodeEditor *e = editorAt(index);
    if (!e)
        return false;

    QString target = path;
    if (target.isEmpty())
        target = e->property(kPropPath).toString();
    if (target.isEmpty())
        return false;                      // 调用方应该先弹另存为

    QFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << e->toPlainText();
    file.close();

    const QString text = e->toPlainText();
    e->setProperty(kPropPath, QFileInfo(target).absoluteFilePath());
    e->setProperty(kPropSavedText, text);
    e->document()->setModified(false);

    setTabToolTip(index, QDir::toNativeSeparators(QFileInfo(target).absoluteFilePath()));
    refreshTabText(index);
    return true;
}

bool EditorTabs::maybeClose(int index)
{
    if (index < 0 || index >= count())
        return true;

    // 空白页
    if (index == 0 && widget(0) == _blank)
        return false;

    CodeEditor *e = editorAt(index);
    if (!e)
        return true;

    // 先记下「当前是哪个」，关完之后如果关的正是当前页，Qt 会自己切走，
    // 但切走的方向不一定友好，这里手动指定一个更合理的目标
    const bool wasCurrent = (currentIndex() == index);

    if (isDirty(index)) {
        setCurrentIndex(index);
        const auto ret = QMessageBox::question(
            this,
            QStringLiteral("保存更改"),
            QStringLiteral("「%1」有未保存的修改，要保存吗？")
                .arg(QFileInfo(e->property(kPropPath).toString()).fileName().isEmpty()
                         ? tabText(index)
                         : QFileInfo(e->property(kPropPath).toString()).fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (ret == QMessageBox::Cancel)
            return false;                  // 用户取消：整个关闭流程中止

        // 选「保存」但还没有磁盘路径：先让用户另存为，失败就放弃关闭
        if (ret == QMessageBox::Save) {
            if (e->property(kPropPath).toString().isEmpty()) {
                const QString path = QFileDialog::getSaveFileName(
                    this, QStringLiteral("保存文件"), QString(),
                    QStringLiteral("所有文件 (*)"));
                if (path.isEmpty() || !saveIndex(index, path))
                    return false;
            } else if (!saveIndex(index)) {
                return false;
            }
        }
    }

    // ⚠️ 关键：removeTab 不会销毁 page，必须自己 deleteLater。
    //    而 deleteLater 是延迟的，所以在这之后绝不能再碰 e。
    removeTab(index);
    e->deleteLater();

    renumberCloseButtons();

    if (wasCurrent && count() > 0) {
        // 关掉当前页后优先落在左边那个，没有就落在第一个
        setCurrentIndex(qBound(0, index - 1, count() - 1));
    }
    if (count() == 1 && widget(0) == _blank)
        setCurrentIndex(0);

    updateBlankState();
    return true;
}

// ---------------------------------------------------------------------------
//  批量关闭：从大到小删，避免下标错位
// ---------------------------------------------------------------------------
bool EditorTabs::closeOthers(int keepIndex)
{
    for (int i = count() - 1; i >= 0; --i) {
        if (i == keepIndex)
            continue;
        if (!maybeClose(i))
            return false;                  // 用户取消，剩下的不动
    }
    if (keepIndex >= 0 && keepIndex < count())
        setCurrentIndex(keepIndex);
    return true;
}

void EditorTabs::closeAll()
{
    closeOthers(-1);
}

bool EditorTabs::closeRight(int fromIndex)
{
    for (int i = count() - 1; i > fromIndex; --i) {
        if (!maybeClose(i))
            return false;
    }
    return true;
}

void EditorTabs::closeCurrent()
{
    const int idx = currentIndex();
    if (idx < 0 || widget(idx) == _blank)
        return;
    maybeClose(idx);
}

void EditorTabs::setCurrentEditor(CodeEditor *editor)
{
    const int idx = indexOfEditor(editor);
    if (idx >= 0)
        setCurrentIndex(idx);
}

// ---------------------------------------------------------------------------
//  槽
// ---------------------------------------------------------------------------
void EditorTabs::onCurrentChanged(int index)
{
    Q_UNUSED(index)
    emit currentEditorChanged(currentEditor());
}

void EditorTabs::onCloseRequested(int index)
{
    if (index < 0 || index >= count())
        return;
    if (widget(index) == _blank)
        return;
    maybeClose(index);
}

// ---------------------------------------------------------------------------
//  标签右键菜单
// ---------------------------------------------------------------------------
void EditorTabs::showTabMenu(const QPoint &pos)
{
    QTabBar *bar = tabBar();
    if (!bar)
        return;

    const int idx = bar->tabAt(pos);
    if (idx < 0)
        return;

    QMenu menu(this);
    QAction *actClose  = menu.addAction(QStringLiteral("关闭"));
    QAction *actOther  = menu.addAction(QStringLiteral("关闭其他"));
    QAction *actRight  = menu.addAction(QStringLiteral("关闭右侧标签"));
    menu.addSeparator();
    QAction *actAll    = menu.addAction(QStringLiteral("关闭所有"));

    const bool isBlank  = (widget(idx) == _blank);
    const int  realTabs = count() - (widget(0) == _blank ? 1 : 0);

    actClose->setEnabled(!isBlank);
    actOther->setEnabled(realTabs > 1);
    actRight->setEnabled(idx < count() - 1);
    actAll->setEnabled(realTabs > 0);

    QAction *chosen = menu.exec(bar->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == actClose)      maybeClose(idx);
    else if (chosen == actOther) closeOthers(idx);
    else if (chosen == actRight) closeRight(idx);
    else if (chosen == actAll)   closeAll();
}

void EditorTabs::updateBlankState()
{
    // 只剩空白页时，把它的关闭按钮再摘一次（removeTab 不会动 0 号）
    if (count() == 1 && widget(0) == _blank) {
        if (QTabBar *bar = tabBar()) {
            bar->setTabButton(0, QTabBar::RightSide, nullptr);
            bar->setTabButton(0, QTabBar::LeftSide, nullptr);
        }
        emit currentEditorChanged(nullptr);
        emit allTabsClosed();
    }
}

int EditorTabs::editorCount() const
{
    int n = 0;
    for (int i = 0; i < count(); ++i)
        if (qobject_cast<CodeEditor *>(widget(i)))
            ++n;
    return n;
}

// ---------------------------------------------------------------------------
//  资源管理器改名 / 删除后的同步
//
//  文件被动过之后，已打开标签里的「filePath」就过期了：
//    - 改名：不移过去的话，之后 Ctrl+S 会把内容写回**旧文件名**，
//            等于"复活"了一个用户刚改走的文件，这是最容易踩的坑。
//    - 删除：标签还留着，再按 Ctrl+S 就会把文件又创建回来。
// ---------------------------------------------------------------------------
void EditorTabs::notifyFileRenamed(const QString &oldPath, const QString &newPath)
{
    if (oldPath.isEmpty() || newPath.isEmpty())
        return;

    const QString oldAbs = QFileInfo(oldPath).absoluteFilePath();
    const QString newAbs = QFileInfo(newPath).absoluteFilePath();

    for (int i = 0; i < count(); ++i) {
        CodeEditor *e = editorAt(i);
        if (!e)
            continue;

        const QString p = QFileInfo(e->property(kPropPath).toString()).absoluteFilePath();
        if (p != oldAbs)
            continue;

        e->setProperty(kPropPath, newAbs);
        setTabToolTip(i, QDir::toNativeSeparators(newAbs));
        refreshTabText(i);
    }
}

void EditorTabs::notifyPathDeleted(const QString &path)
{
    if (path.isEmpty())
        return;

    const QFileInfo target(path);
    const QString targetAbs = target.absoluteFilePath();
    const bool isDir = target.isDir();

    // 倒序：removeTab 会让后面的下标前移
    for (int i = count() - 1; i >= 0; --i) {
        CodeEditor *e = editorAt(i);
        if (!e)
            continue;

        const QString p = QFileInfo(e->property(kPropPath).toString()).absoluteFilePath();
        if (p.isEmpty())
            continue;

        // 删的是目录时，目录下的所有文件标签也要一起关
        const bool hit = (p == targetAbs)
                         || (isDir && p.startsWith(targetAbs + QLatin1Char('/')));
        if (!hit)
            continue;

        removeTab(i);
        e->deleteLater();
    }

    renumberCloseButtons();

    if (count() == 1 && widget(0) == _blank)
        emit currentEditorChanged(nullptr);
}

bool EditorTabs::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _blank && event->type() == QEvent::MouseButtonRelease) {
        emit blankPageClicked();
    }
    return QTabWidget::eventFilter(watched, event);
}
