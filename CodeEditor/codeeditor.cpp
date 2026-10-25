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

    connect(this,&QPlainTextEdit::cursorPositionChanged,this,&CodeEditor::onCursorPositionChanged);;
    connect(this,&QPlainTextEdit::updateRequest,this,&CodeEditor::onUpdateRequest);
    connect(this,&QPlainTextEdit::blockCountChanged,this,&CodeEditor::onBlockCountChanged);

    updateGutterWidth();
}

int CodeEditor::lineNumberAreaWidth() const
{
    static constexpr int width = 13;
    static constexpr int cand[] = {
        10,100,1000,10000,100000,1000000,10000000,100000000
    };
    int i = 1;
    for(const auto&c:cand){
        if (c >=blockCount() ){
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

    const int gutter_w = _gutter->width();
    while(block.isValid() && top <=event->rect().bottom() ){
        if (block.isVisible() && bottom >= event->rect().top()){
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

void CodeEditor::updateGutterWidth()
{
    const int w = lineNumberAreaWidth();
    if (viewportMargins().left()<w){
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

    setExtraSelections(selections);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    updateGutterWidth();
    // const QRect cr = contentsRect();
    // const QRect cr = viewport()->rect();
    // _gutter->setGeometry(QRect(cr.left(),cr.top(),lineNumberAreaWidth(),cr.height()));
}
