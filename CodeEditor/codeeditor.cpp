#include "codeeditor.h"
#include "codehighlighter.h"
#include "linenumberarea.h"
#include "theme.h"

#include <QWidget>
#include <QTextBlock>
#include <QPainter>
#include <QtOpenGLWidgets/QtOpenGLWidgets>

namespace {
constexpr int kDocumentMargin = 5;
constexpr int kFoldMarkerWidth = 5;   // 左边这么宽的区域属于折叠三角
static const QString kIndentUnit = QStringLiteral("    ");   // 4 空格缩进单位
}

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    /*
     *  默认构造函数，设置默认的一些设置，之后可以提供接口进行用户自定义修改
        也可以添加QSettting进行保存，下次可以复现。
    */
    _gutter = new LineNumberArea(this);

    // 字体
    const QFont mono = Theme::monoFont(11);
    setFont(mono);
    document() -> setDefaultFont(mono);

    // tab宽度
    setTabStopDistance(4 * QFontMetricsF(mono).horizontalAdvance(QLatin1Char(' ')));

    // 光标宽度
    setCursorWidth(2);
    // 去掉控件自带的边框
    setFrameShape(QFrame::NoFrame);
    // 默认不折行，靠横向滚动条
    setLineWrapMode(QPlainTextEdit::NoWrap);
    // 文字离视口边缘留一点空
    document()->setDocumentMargin(kDocumentMargin);

    // ---- 调色板（不用 stylesheet，避免和 ExtraSelection 打架）
    QPalette pal = palette();
    pal.setColor(QPalette::Base,            Theme::kEditorBg);
    pal.setColor(QPalette::Text,            Theme::kEditorFg);
    pal.setColor(QPalette::Highlight,       Theme::kSelectionBg);
    pal.setColor(QPalette::HighlightedText, Theme::kEditorFg);
    setPalette(pal);

    _highlighter = new CodeHighlighter(document());
    _folds.setDocument(document());

    connect(this,&QPlainTextEdit::cursorPositionChanged,this,&CodeEditor::onCursorPositionChanged);;
    connect(this,&QPlainTextEdit::updateRequest,this,&CodeEditor::onUpdateRequest);
    connect(this,&QPlainTextEdit::blockCountChanged,this,&CodeEditor::onBlockCountChanged);
    connect(this, &QPlainTextEdit::textChanged,         this, &CodeEditor::onTextChanged);
    connect(document(), &QTextDocument::contentsChange, this, &CodeEditor::onContentsChange);

    updateGutterWidth();
}

int CodeEditor::lineNumberAreaWidth() const
{
    static constexpr int width = 13;
    static constexpr int cand[] = {
        1,10,100,1000,10000,100000,1000000,10000000,100000000
    };
    int i = 1;
    for(const auto&c:cand){
        if (c >= blockCount() ){
            return kFoldMarkerWidth+ i * width+10;
        }
        i++;
    }
    return 1000000000;
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(_gutter);
    painter.fillRect(event->rect(),Theme::kGutterBg);

    QTextBlock block = firstVisibleBlock();
    if (!block.isValid()){
        return;
    }

    int blockNumber = block.blockNumber();
    const int lineHeight = fontMetrics().height();   // 固定的行高
    /*
        blockBoundingGeometry(block) : 块在文档内容坐标系里的位置。它是绝对的、不变的
        contentOffset() —— 这不是"内容滑动后的位置"，而是"视口原点相对内容原点的偏移量"。它描述的是"你把镜头往文档里推了多远"，是一个偏移量/差值，不是一个位置。
    */
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int height = qRound(blockBoundingRect(block).height());
    int bottom = top+height;

    const int currentLine = textCursor().blockNumber();
    const QFont baseFont  = font();

    const int gutter_w = _gutter->width();
    while(block.isValid() && top <=event->rect().bottom() ){
        if (block.isVisible() && bottom >= event->rect().top()){

            if (_folds.hasFoldAt(blockNumber)){
                const bool folded = _folds.isCollapsedAt(blockNumber);
                const bool hot = (blockNumber == _hoverBlock);

                painter.setPen(Qt::NoPen);
                painter.setBrush(hot ? Theme::kFoldArrowHot : Theme::kFoldArrow);

                const qreal cx = 9.0;
                const qreal cy = top + height / 2.0;

                QPolygonF tri;
                if (folded){
                    tri << QPointF(cx - 3, cy - 5) << QPointF(cx + 4, cy) << QPointF(cx - 3, cy + 5);
                }else{
                    tri << QPointF(cx - 5, cy - 3) << QPointF(cx + 5, cy - 3) << QPointF(cx, cy + 4);
                }
                painter.drawPolygon(tri);
            }
            const bool isCurrent = (blockNumber == currentLine);
            QFont f = baseFont;
            f.setBold(isCurrent);
            painter.setFont(f);
            painter.setPen(Theme::kGutterText);
            painter.drawText(kFoldMarkerWidth,top,gutter_w ,lineHeight,Qt::AlignHCenter | Qt::AlignVCenter,QString::number(blockNumber + 1));
        }

        block = block.next();
        top = bottom;
        height = block.isValid() ? qRound(blockBoundingRect(block).height()) : 0;
        bottom = top+height;
        ++blockNumber;
    }

    painter.setPen(Theme::kGutterBorder);
    painter.drawLine(gutter_w - 1,event->rect().top(),gutter_w - 1,event->rect().bottom());
}

void CodeEditor::handleGutterClick(const QPoint &pos)
{
    const int bn = blockNumberAtViewportY(pos.y());
    if (bn < 0)
        return;

    if (pos.x() <= kFoldMarkerWidth + 20 && _folds.hasFoldAt(bn)) {
        _folds.toggle(bn);

        // 折叠后把光标移到这一行，否则光标还留在被隐藏的区域里，
        // 下一帧的 revealCursor() 会立刻把它展开回来。

        QTextCursor c(document()->findBlockByNumber(bn));
        setTextCursor(c);

        _gutter->update();
        viewport()->update();
        emit foldStateChanged();
        return;
    }

    QTextCursor c(document()->findBlockByNumber(bn));
    setTextCursor(c);
    setFocus();
}

void CodeEditor::handleGutterHover(const QPoint &pos)
{
    int bn = -1;
    if (pos.x() >= 0 && pos.x() <= kFoldMarkerWidth + 20)
        bn = blockNumberAtViewportY(pos.y());

    const int newHover = (bn >= 0 && _folds.hasFoldAt(bn)) ? bn : -1;
    if (newHover != _hoverBlock) {
        _hoverBlock = newHover;
        _gutter->update();
        // 三角上是「手型」，其它地方是普通箭头（行号栏不该出现文本光标）
        _gutter->setCursor(newHover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void CodeEditor::toggleFoldAtCursor()
{
    const int bn = textCursor().blockNumber();
    int target = -1;
    if (_folds.hasFoldAt(bn)){
        target = bn;
    }else{
        for(const FoldManager::Region &r:_folds.regions()){
            if (r.startBlock <= bn && bn <= r.endBlock){
                target = qMax(target,r.startBlock);
            }
        }
    }
    if (target < 0){
        return;
    }
    _folds.toggle(target);
    QTextCursor c (document()->findBlockByNumber(target));
    setTextCursor(c);

    _gutter->update();
    viewport()->update();
    emit foldStateChanged();

}

void CodeEditor::foldAll()
{
    _folds.foldAll();
    _gutter->update();
    viewport()->update();
    emit foldStateChanged();
}

void CodeEditor::unfoldAll()
{
    _folds.unfoldAll();
    _gutter->update();
    viewport()->update();
    emit foldStateChanged();
}

bool CodeEditor::foldsAreCollapsed(int blockNumber) const
{
    return _folds.isCollapsedAt(blockNumber);
}

/*
    设置当前的指向行号
*/
void CodeEditor::gotoline(int lineNumber)
{
    const int n = qBound(1,lineNumber,qMax(1,blockCount()));
    QTextBlock b = document() -> findBlockByLineNumber(n-1);
    if (!b.isValid()){
        return;
    }
    QTextCursor c(b);
    c.movePosition(QTextCursor::StartOfBlock);
    setTextCursor(c);
    centerCursor();
    setFocus();
}

/*
    设置是否折行
*/
void CodeEditor::setWordWrapEnabled(bool on)
{
    setLineWrapMode( on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    updateGutterWidth();
}

QString CodeEditor::statsText() const
{
    return QStringLiteral("%1 行  %2 字符")
        .arg(blockCount())
        .arg(document()->characterCount() - 1);
}

/*
    光标变化->告诉外部 目前几行几列
*/
void CodeEditor::onCursorPositionChanged()
{
    refreshExtraSelections();
    if (_gutter) _gutter->update();
    const QTextCursor c = textCursor();
    emit cursorMoved(c.blockNumber() + 1,c.positionInBlock() + 1);
}

void CodeEditor::onUpdateRequest(const QRect &rect, int dy)
{
    if (!_gutter)return;

    if (dy != 0){
        _gutter->scroll(0,dy);
    }else{
        _gutter->update(0,rect.y(),_gutter->width(),_gutter->height());
    }

    if (rect.contains(viewport()->rect())){
        updateGutterWidth();
    }
}

void CodeEditor::onBlockCountChanged(int newBlockCount)
{
    updateGutterWidth();
}

void CodeEditor::onTextChanged()
{
    if (_rescanning)
        return;

    //增量判断
    //    ① 行数变了（插入/删除换行 → 后面所有 blockNumber 都会错位）
    //    ② 删掉了东西（可能是 }，无法从文档里回溯，保守重扫）
    //    ③ 新插入的文本里出现 { 或 }

    const int blockNow = blockCount();
    const bool structural = (blockNow != _lastBlockCount)
                            || (_lastRemoved > 0)
                            || _lastAddedText.contains(QLatin1Char('{'))
                            || _lastAddedText.contains(QLatin1Char('}'));
    _lastBlockCount = blockNow;

    if (!structural)return;

    _rescanning = true;
    _folds.rescan();
    _rescanning = false;

    updateGutterWidth();
    if (_gutter)_gutter->update();
    viewport()->update();
    emit foldStateChanged();
}

void CodeEditor::updateGutterWidth()
{
    const int w = lineNumberAreaWidth();
    if (viewportMargins().left()!=w){
        setViewportMargins(w,0,0,0);
    }
    if (_gutter){
        const QRect cr = contentsRect();
        _gutter->setGeometry(QRect(cr.left(),cr.top(),w,cr.height()));
    }
}

void CodeEditor::refreshExtraSelections()
{
    QList<QTextEdit::ExtraSelection>selections;
    if (!isReadOnly()){
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(Theme::kCurrentLine);
        sel.format.setProperty(QTextFormat::FullWidthSelection,true);
        sel.cursor = textCursor();
        sel.cursor.clearSelection();
        selections.append(sel);
    }

    _brackPair = BracketMatcher::match(document(),textCursor().position());
    if (_brackPair.valid){
        const int positions[2] = {
            _brackPair.nearPos,_brackPair.matePos
        };
        for(int pos : positions){
            if (pos < 0 ){
                continue;
            }

            const QTextBlock b = document()->findBlock(pos);
            if (!b.isValid() || !b.isVisible()){
                continue;
            }

            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(Theme::kMatchBg);
            sel.format.setForeground(Theme::kMatchFg);
            sel.format.setFontWeight(QFont::Bold);
            QTextCursor c(document());
            c.setPosition(pos);
            c.setPosition(pos+1,QTextCursor::KeepAnchor);
            sel.cursor = c;
            selections.append(sel);
        }
    }

    setExtraSelections(selections);
}

void CodeEditor::revealCursor()
{
    const QTextBlock b = textCursor().block();
    if (!b.isValid() || b.isVisible())
        return;

    bool changed = false;
    // 最多解 16 层，防止畸形文档导致死循环
    for (int guard = 0; guard < 16; ++guard) {
        const int idx = _folds.collapsedRegionContaining(b.blockNumber());
        if (idx < 0)
            break;
        _folds.setCollapsed(_folds.regions().at(idx).startBlock, false);
        changed = true;
    }

    if (changed) {
        if (_gutter) _gutter->update();
        emit foldStateChanged();
    }
}

void CodeEditor::drawFoldHints(QPainter &painter)
{
    painter.setRenderHints(QPainter::Antialiasing,true);

    QFont f = font();
    f.setPointSizeF(f.pointSizeF() -1.0);
    f.setItalic(true);
    painter.setFont(f);
    const  QFontMetrics fm(f);

    const QRect vp = viewport()->rect();
    for(const FoldManager::Region &r : _folds.regions()){
        if (!r.collapsed){
            continue;
        }
        QTextBlock block = document() -> findBlockByNumber(r.startBlock);
        if (!block.isValid() || !block.isVisible()){
            continue;
        }

        const QRect br = blockBoundingGeometry(block).translated(contentOffset()).toRect();
        // 实际的内容矩形和视口矩形有任何交叠就返回true
        if (!br.intersects(vp))
            continue;

        int textWidth = 0;
        if (QTextLayout *layout = block.layout()){
            for(int i = 0 ; i < layout->lineCount();++i){
                textWidth = qMax(textWidth, qRound(layout->lineAt(i).naturalTextWidth()));
            }
        }

        const QString hint  = QString::fromUtf8("··· %1 行").arg(r.hiddenLines());
        const int     hintW = fm.horizontalAdvance(hint) + 16;
        const int     x     = br.left() + textWidth + 10;
        if (x + hintW > vp.width())
            continue;                       // 放不下就不画，别溢出视口

        const QRectF box(x,br.top()+2,hintW,br.height() - 4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Theme::kFoldHintBg);
        painter.drawRoundedRect(box, 4, 4);

        painter.setPen(Theme::kFoldHintFg);
        painter.drawText(box, Qt::AlignCenter, hint);
    }
}

void CodeEditor::onContentsChange(int position, int charsRemoved, int charsAdded)
{

    _lastRemoved = charsRemoved;

    _lastAddedText.clear();
    if (charsAdded > 0) {
        QTextCursor c(document());
        c.setPosition(position);
        c.setPosition(position + charsAdded, QTextCursor::KeepAnchor);
        _lastAddedText = c.selectedText();
    }

}

int CodeEditor::blockNumberAtViewportY(int y) const
{
    QTextBlock block = firstVisibleBlock();
    if (!block.isValid()){
        return -1;
    }
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).y());
    while(block.isValid()){
        const int h = qRound(blockBoundingRect(block).height());
        if (y>=top && y < top+h){
            return block.blockNumber();
        }
        top+=h;
        block = block.next();
    }
    return -1;
}

QRect CodeEditor::blockRectInViewport(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()){
        return QRect();
    }
    return blockBoundingGeometry(block).translated(contentOffset()).toRect();
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    updateGutterWidth();
    // const QRect cr = contentsRect();
    // const QRect cr = viewport()->rect();
    // _gutter->setGeometry(QRect(cr.left(),cr.top(),lineNumberAreaWidth(),cr.height()));
}

void CodeEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);
    QPainter painter(viewport());
    drawFoldHints(painter);
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    const bool plainReturn = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    && (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;

    if (event->key() == Qt::Key_Return && (event->modifiers() & Qt::ControlModifier)){
        QTextCursor c = textCursor();
        c.movePosition(QTextCursor::EndOfBlock);
        c.insertBlock();
        setTextCursor(c);
        return;
    }


    if (event->key() == Qt::Key_Tab && event->modifiers() == Qt::NoModifier) {
        insertPlainText(QStringLiteral("    "));   // 插入 4 个空格而不是制表符
        return;
    }

    if (plainReturn) {
        QTextCursor cursor = textCursor();
        const QString lineText = cursor.block().text();

        // 行首缩进（连续空格或 \t）
        int i = 0;
        while (i < lineText.size()
               && (lineText.at(i) == QLatin1Char(' ') || lineText.at(i) == QLatin1Char('\t')))
            ++i;
        QString indent = lineText.left(i);

        // 上一行以 { 结尾 → 新行多一级
        const QString trimmed = lineText.trimmed();
        if (!trimmed.isEmpty() && trimmed.back() == QLatin1Char('{'))
            indent += kIndentUnit;

        cursor.beginEditBlock();
        cursor.insertText(QStringLiteral("\n") + indent);
        cursor.endEditBlock();
        setTextCursor(cursor);
        event->accept();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}
