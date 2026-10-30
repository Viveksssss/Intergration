#include "CodeEditor.h"

#include "CodeHighlighter.h"
#include "LineNumberArea.h"
#include "CodeTheme.h"

#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>

// ===========================================================================
//  构造
// ===========================================================================
CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_gutter = new LineNumberArea(this);

    // ---- 字体：整段代码的排版都建立在这个字体上
    const QFont mono = CodeTheme::monoFont(11);
    setFont(mono);
    document()->setDefaultFont(mono);
    // Tab 键的宽度 = 4 个空格。setTabStopDistance 用的是像素，不是字符数。
    setTabStopDistance(4 * QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')));

    setCursorWidth(2);
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::NoWrap);   // 代码不折行，靠横向滚动条
    document()->setDocumentMargin(6);          // 文字离视口边缘留一点空

    // ---- 调色板（不用 stylesheet，避免和 ExtraSelection 打架）
    QPalette pal = palette();
    pal.setColor(QPalette::Base,            CodeTheme::kEditorBg);
    pal.setColor(QPalette::Text,            CodeTheme::kEditorFg);
    pal.setColor(QPalette::Highlight,       CodeTheme::kSelectionBg);
    pal.setColor(QPalette::HighlightedText, CodeTheme::kEditorFg);
    setPalette(pal);

    m_highlighter = new CodeHighlighter(document());
    m_folds.setDocument(document());

    // ---- 行号栏同步的三个钩子（Qt 官方 Code Editor 示例就是这三条）
    connect(this, &QPlainTextEdit::blockCountChanged,   this, &CodeEditor::onBlockCountChanged);
    connect(this, &QPlainTextEdit::updateRequest,       this, &CodeEditor::onUpdateRequest);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::onCursorPositionChanged);
    connect(this, &QPlainTextEdit::textChanged,         this, &CodeEditor::onTextChanged);
    connect(document(), &QTextDocument::contentsChange, this, &CodeEditor::onContentsChange);

    updateGutterWidth();
}

// ===========================================================================
//  一、行号栏的宽度
// ===========================================================================
int CodeEditor::lineNumberAreaWidth() const
{
    // 位数要按「最多可能有多少行」算，而不是当前行数。
    // 否则从 99 行变到 100 行的一瞬间，行号栏会突然变宽，整段文字左右跳一下。
    int lines  = qMax(1, blockCount());
    int digits = 1;
    while (lines >= 10) { lines /= 10; ++digits; }

    const int digitW = fontMetrics().horizontalAdvance(QLatin1Char('9'));
    const int marker = 18;   // 左边留给折叠三角的槽位
    const int pad    = 10;   // 行号和右边框之间
    return marker + digits * digitW + pad;
}

void CodeEditor::updateGutterWidth()
{
    const int w = lineNumberAreaWidth();
    if (viewportMargins().left() != w)
        setViewportMargins(w, 0, 0, 0);

    if (m_gutter) {
        const QRect cr = contentsRect();
        m_gutter->setGeometry(QRect(cr.left(), cr.top(), w, cr.height()));
    }
}

void CodeEditor::onBlockCountChanged(int)
{
    updateGutterWidth();
}

// ===========================================================================
//  二、行号栏与滚动的同步（本文件最容易写错的地方）
// ===========================================================================
void CodeEditor::onUpdateRequest(const QRect &rect, int dy)
{
    if (!m_gutter) return;

    // dy != 0 表示「内容整体上/下平移了 dy 像素」，也就是滚动。
    // 这种情况只需要把行号栏也平移同样像素，不重画 —— 画了反而会闪。
    //
    // dy == 0 表示「这块区域的内容变了，需要重绘」，
    // rect 是 viewport 坐标，直接拿来当行号栏的重绘区（两者 y 轴是对齐的）。
    if (dy != 0)
        m_gutter->scroll(0, dy);
    else
        m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());

    // 整块 viewport 都失效时，宽度也可能变了（比如换行模式），一起更新
    if (rect.contains(viewport()->rect()))
        updateGutterWidth();
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    const QRect cr = contentsRect();
    m_gutter->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// ===========================================================================
//  三、画行号栏
// ===========================================================================
void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_gutter);
    painter.fillRect(event->rect(), CodeTheme::kGutterBg);

    QTextBlock block = firstVisibleBlock();
    if (!block.isValid())
        return;

    // 关键三件套：
    //   blockBoundingGeometry(block) —— block 在「内容坐标」里的矩形
    //   contentOffset()              —— 当前内容被滚动了多少
    //   两者相加                     —— block 在 viewport 里的位置
    //
    // 隐藏的 block（折叠掉的）blockBoundingRect 高度是 0，
    // 所以下面 top/bottom 的累加天然会跳过它们，不用额外判断。
    int blockNumber = block.blockNumber();
    int top    = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int height = qRound(blockBoundingRect(block).height());
    int bottom = top + height;

    const int currentLine = textCursor().blockNumber();
    const int gutterW     = m_gutter->width();
    const QFont baseFont  = font();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {

            // ---- 折叠三角
            if (m_folds.hasFoldAt(blockNumber)) {
                const bool folded = m_folds.isCollapsedAt(blockNumber);
                const bool hot    = (blockNumber == m_hoverBlock);

                painter.setPen(Qt::NoPen);
                painter.setBrush(hot ? CodeTheme::kFoldArrowHot : CodeTheme::kFoldArrow);

                const qreal cx = 9.0;
                const qreal cy = top + height / 2.0;
                QPolygonF tri;
                if (folded)   // ▸
                    tri << QPointF(cx - 3, cy - 5) << QPointF(cx + 4, cy) << QPointF(cx - 3, cy + 5);
                else          // ▾
                    tri << QPointF(cx - 5, cy - 3) << QPointF(cx + 5, cy - 3) << QPointF(cx, cy + 4);
                painter.drawPolygon(tri);
            }

            // ---- 行号
            const bool isCurrent = (blockNumber == currentLine);
            QFont f = baseFont;
            f.setBold(isCurrent);
            painter.setFont(f);
            painter.setPen(isCurrent ? CodeTheme::kGutterCurrent : CodeTheme::kGutterText);

            painter.drawText(18, top, gutterW - 18 - 8, height,
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(blockNumber + 1));
        }

        block  = block.next();
        top    = bottom;
        height = block.isValid() ? qRound(blockBoundingRect(block).height()) : 0;
        bottom = top + height;
        ++blockNumber;
    }

    // 右侧一条竖线，把行号栏和正文分开
    painter.setPen(CodeTheme::kGutterBorder);
    painter.drawLine(gutterW - 1, event->rect().top(), gutterW - 1, event->rect().bottom());
}

// ===========================================================================
//  四、行号栏上的鼠标交互
// ===========================================================================
namespace {
constexpr int kFoldMarkerWidth = 18;   // 左边这么宽的区域属于折叠三角
}

int CodeEditor::blockNumberAtViewportY(int y) const
{
    QTextBlock block = firstVisibleBlock();
    if (!block.isValid())
        return -1;

    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    while (block.isValid()) {
        const int h = qRound(blockBoundingRect(block).height());
        if (y >= top && y < top + h)
            return block.blockNumber();
        top += h;
        block = block.next();
    }
    return -1;
}

QRect CodeEditor::blockRectInViewport(int blockNumber) const
{
    QTextBlock b = document()->findBlockByNumber(blockNumber);
    if (!b.isValid())
        return QRect();
    return blockBoundingGeometry(b).translated(contentOffset()).toRect();
}

void CodeEditor::handleGutterClick(const QPoint &pos)
{
    const int bn = blockNumberAtViewportY(pos.y());
    if (bn < 0)
        return;

    if (pos.x() <= kFoldMarkerWidth && m_folds.hasFoldAt(bn)) {
        m_folds.toggle(bn);

        // 折叠后把光标移到这一行，否则光标还留在被隐藏的区域里，
        // 下一帧的 revealCursor() 会立刻把它展开回来。
        QTextCursor c(document()->findBlockByNumber(bn));
        setTextCursor(c);

        rehighlightVisible();
        m_gutter->update();
        viewport()->update();
        emit foldStateChanged();
        return;
    }

    // 点行号的空白处：把光标移到这一行开头
    QTextCursor c(document()->findBlockByNumber(bn));
    setTextCursor(c);
    setFocus();
}

void CodeEditor::handleGutterHover(const QPoint &pos)
{
    int bn = -1;
    if (pos.x() >= 0 && pos.x() <= kFoldMarkerWidth)
        bn = blockNumberAtViewportY(pos.y());

    const int newHover = (bn >= 0 && m_folds.hasFoldAt(bn)) ? bn : -1;
    if (newHover != m_hoverBlock) {
        m_hoverBlock = newHover;
        m_gutter->update();
        // 三角上是「手型」，其它地方是普通箭头（行号栏不该出现文本光标）
        m_gutter->setCursor(newHover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

// ===========================================================================
//  五、光标变化 → 当前行高亮 + 括号匹配
// ===========================================================================
void CodeEditor::onCursorPositionChanged()
{
    revealCursor();
    refreshExtraSelections();
    if (m_gutter) m_gutter->update();

    const QTextCursor c = textCursor();
    emit cursorMoved(c.blockNumber() + 1, c.positionInBlock() + 1);
}

void CodeEditor::refreshExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;

    // ---- ① 当前行整行底色
    // FullWidthSelection 的作用是：不管这段文字多长，底色都从行首一直铺到视口右边缘。
    // 如果不设它，只有「有字符的那一段」会被涂色，右边留一条白 —— 很难看。
    if (!isReadOnly()) {
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(CodeTheme::kCurrentLine);
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        selections.append(sel);
    }

    // ---- ② 括号匹配
    // 只在光标移动时算一次，结果缓存下来（m_bracketPair），不参与绘制循环。
    m_bracketPair = BracketMatcher::match(document(), textCursor().position());

    if (m_bracketPair.valid) {
        const int positions[2] = { m_bracketPair.nearPos, m_bracketPair.matePos };
        for (int pos : positions) {
            if (pos < 0)
                continue;

            // 配对的那个括号可能在折叠区里（被 setVisible(false) 了）。
            // 这种情况不画，否则会在一个看不见的地方涂色。
            const QTextBlock b = document()->findBlock(pos);
            if (!b.isValid() || !b.isVisible())
                continue;

            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(CodeTheme::kMatchBg);
            sel.format.setForeground(CodeTheme::kMatchFg);
            sel.format.setFontWeight(QFont::Bold);
            QTextCursor c(document());
            c.setPosition(pos);
            c.setPosition(pos + 1, QTextCursor::KeepAnchor);
            sel.cursor = c;
            selections.append(sel);
        }
    }

    // 一次全设进去。分多次调用 setExtraSelections 会闪。
    setExtraSelections(selections);
}

// ===========================================================================
//  六、折叠
// ===========================================================================
void CodeEditor::onTextChanged()
{
    if (m_rescanning)
        return;

    // ------------------------------------------------------------------
    //  增量判断：这一下改动有没有可能改变折叠区的结构？
    //
    //  「结构变了」只有三种情况：
    //    ① 行数变了（插入/删除换行 → 后面所有 blockNumber 都会错位）
    //    ② 删掉了东西（可能是 }，无法从文档里回溯，保守重扫）
    //    ③ 新插入的文本里出现 { 或 }
    //
    //  普通打字（输入字母、空格、数字）三者都不满足，直接跳过。
    //  这是「打字不卡」的关键：全文扫一遍 11600 个 block 要 7ms 左右，
    //  每敲一个键都做的话，手感就废了。
    // ------------------------------------------------------------------
    const int blockNow = blockCount();
    const bool structural = (blockNow != m_lastBlockCount)
                            || (m_lastRemoved > 0)
                            || m_lastAddedText.contains(QLatin1Char('{'))
                            || m_lastAddedText.contains(QLatin1Char('}'));
    m_lastBlockCount = blockNow;

    if (!structural)
        return;

    m_rescanning = true;
    m_folds.rescan();          // 内部会按需 setVisible
    m_rescanning = false;

    updateGutterWidth();
    if (m_gutter) m_gutter->update();
    viewport()->update();
    emit foldStateChanged();
}

// QTextDocument::contentsChange 比 textChanged 多给两个信息：
// 改了什么位置、删了几个字符、加了几个字符。用它来捕获「刚输入的是什么」。
// 注意它发出来时文档内容已经改完了，所以可以直接按新位置取文本。
void CodeEditor::onContentsChange(int position, int charsRemoved, int charsAdded)
{
    m_lastRemoved = charsRemoved;

    m_lastAddedText.clear();
    if (charsAdded > 0) {
        QTextCursor c(document());
        c.setPosition(position);
        c.setPosition(position + charsAdded, QTextCursor::KeepAnchor);
        m_lastAddedText = c.selectedText();
    }
}

void CodeEditor::revealCursor()
{
    const QTextBlock b = textCursor().block();
    if (!b.isValid() || b.isVisible())
        return;

    bool changed = false;
    // 最多解 16 层，防止畸形文档导致死循环
    for (int guard = 0; guard < 16; ++guard) {
        const int idx = m_folds.collapsedRegionContaining(b.blockNumber());
        if (idx < 0)
            break;
        m_folds.setCollapsed(m_folds.regions().at(idx).startBlock, false);
        changed = true;
    }

    if (changed) {
        if (m_gutter) m_gutter->update();
        emit foldStateChanged();
    }
}

void CodeEditor::toggleFoldAtCursor()
{
    const int bn = textCursor().blockNumber();

    int target = -1;
    if (m_folds.hasFoldAt(bn)) {
        target = bn;
    } else {
        // 光标在某段代码中间：折叠它所属的最内层区域
        for (const FoldManager::Region &r : m_folds.regions())
            if (r.startBlock <= bn && bn <= r.endBlock)
                target = qMax(target, r.startBlock);
    }
    if (target < 0)
        return;

    m_folds.toggle(target);

    QTextCursor c(document()->findBlockByNumber(target));
    setTextCursor(c);

    rehighlightVisible();
    m_gutter->update();
    viewport()->update();
    emit foldStateChanged();
}

void CodeEditor::foldAll()
{
    m_folds.foldAll();
    rehighlightVisible();
    m_gutter->update();
    viewport()->update();
    emit foldStateChanged();
}

void CodeEditor::unfoldAll()
{
    m_folds.unfoldAll();
    rehighlightVisible();
    m_gutter->update();
    viewport()->update();
    emit foldStateChanged();
}

bool CodeEditor::foldsAreCollapsed(int blockNumber) const
{
    return m_folds.isCollapsedAt(blockNumber);
}

// 折叠会让 QTextDocument 重建部分 block 的 layout，layout 里存的
// 语法着色格式会跟着一起丢掉。这里只把「当前看得见的几十行」重刷一遍，
// 代价很小；全文 rehighlight() 在十万行文件上会明显卡顿。
void CodeEditor::rehighlightVisible()
{
    if (!m_highlighter)
        return;

    QTextBlock block = firstVisibleBlock();
    int guard = 0;
    while (block.isValid() && guard++ < 300) {
        m_highlighter->rehighlightBlock(block);
        block = block.next();
    }
}

// ===========================================================================
//  七、折叠提示条：在文字层「之上」再画一层
// ===========================================================================
void CodeEditor::paintEvent(QPaintEvent *event)
{
    // 先让 QPlainTextEdit 把该画的都画完（文字、当前行、括号匹配、选区）
    QPlainTextEdit::paintEvent(event);

    // 再用一个新的 QPainter 在 viewport 上叠一层。
    // 此时基类的 painter 已经析构，同一个 widget 上开新 painter 是安全的。
    QPainter painter(viewport());
    drawFoldHints(painter);
}

void CodeEditor::drawFoldHints(QPainter &painter)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont f = font();
    f.setPointSizeF(f.pointSizeF() - 1.0);
    f.setItalic(true);
    painter.setFont(f);
    const QFontMetrics fm(f);

    const QRect vp = viewport()->rect();

    for (const FoldManager::Region &r : m_folds.regions()) {
        if (!r.collapsed)
            continue;

        QTextBlock b = document()->findBlockByNumber(r.startBlock);
        if (!b.isValid() || !b.isVisible())
            continue;

        const QRect br = blockBoundingGeometry(b).translated(contentOffset()).toRect();
        if (!br.intersects(vp))
            continue;                       // 不在视口里，跳过

        // 这一行文字实际画到哪儿了 —— 提示条要紧跟在文字后面
        int textWidth = 0;
        if (QTextLayout *layout = b.layout()) {
            for (int i = 0; i < layout->lineCount(); ++i)
                textWidth = qMax(textWidth, qRound(layout->lineAt(i).naturalTextWidth()));
        }

        const QString hint  = QString::fromUtf8("⋯ %1 行").arg(r.hiddenLines());
        const int     hintW = fm.horizontalAdvance(hint) + 16;
        const int     x     = br.left() + textWidth + 10;

        if (x + hintW > vp.width())
            continue;                       // 放不下就不画，别溢出视口

        const QRectF box(x, br.top() + 2, hintW, br.height() - 4);

        painter.setPen(Qt::NoPen);
        painter.setBrush(CodeTheme::kFoldHintBg);
        painter.drawRoundedRect(box, 4, 4);

        painter.setPen(CodeTheme::kFoldHintFg);
        painter.drawText(box, Qt::AlignCenter, hint);
    }
}

// ===========================================================================
//  八、键盘：Tab 缩进 / Enter 自动缩进
// ===========================================================================
void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    const bool plainReturn = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
                             && (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;

    if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
        insertPlainText(QStringLiteral("    "));   // 插入 4 个空格而不是制表符
        return;
    }

    if (plainReturn) {
        QTextCursor c = textCursor();
        const QString line = c.block().text();
        const int posInBlock = c.positionInBlock();

        // 复制当前行的前导空白
        QString indent;
        for (const QChar ch : line) {
            if (ch == QLatin1Char(' ') || ch == QLatin1Char('\t'))
                indent += ch;
            else
                break;
        }

        // 行尾是 { 就多缩进一级
        if (line.trimmed().endsWith(QLatin1Char('{')))
            indent += QStringLiteral("    ");

        // 「{} 之间回车」的贴心处理：拆成三行，光标停在中间那行
        const bool insideBraces = posInBlock > 0
                                  && line.at(posInBlock - 1) == QLatin1Char('{')
                                  && line.mid(posInBlock).trimmed().startsWith(QLatin1Char('}'));

        if (insideBraces) {
            QString outer = indent;
            if (outer.endsWith(QStringLiteral("    ")))
                outer.chop(4);
            c.beginEditBlock();
            c.insertText(QStringLiteral("\n") + indent + QStringLiteral("\n") + outer);
            c.endEditBlock();
            // 光标回退到中间那行
            c.movePosition(QTextCursor::PreviousBlock);
            c.movePosition(QTextCursor::EndOfBlock);
            setTextCursor(c);
            return;
        }

        c.beginEditBlock();
        c.insertText(QStringLiteral("\n") + indent);
        c.endEditBlock();
        setTextCursor(c);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

// ===========================================================================
//  九、其它工具方法
// ===========================================================================
void CodeEditor::gotoLine(int lineNumber)
{
    const int n = qBound(1, lineNumber, qMax(1, blockCount()));
    QTextBlock b = document()->findBlockByNumber(n - 1);
    if (!b.isValid())
        return;

    // 目标行可能被折叠了，先解开
    if (!b.isVisible()) {
        const int idx = m_folds.collapsedRegionContaining(b.blockNumber());
        if (idx >= 0)
            m_folds.setCollapsed(m_folds.regions().at(idx).startBlock, false);
    }

    QTextCursor c(b);
    c.movePosition(QTextCursor::StartOfBlock);
    setTextCursor(c);
    centerCursor();
    setFocus();
}

void CodeEditor::setWordWrapEnabled(bool on)
{
    setLineWrapMode(on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    updateGutterWidth();
}

QString CodeEditor::statsText() const
{
    return QStringLiteral("%1 行   %2 字符")
        .arg(blockCount())
        .arg(document()->characterCount() - 1);
}
