#include "codehighlighter.h"
#include "theme.h"
#include <QTextDocument>

CodeHighlighter::CodeHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    _commentFmt.setForeground(Theme::kComment);
    _commentFmt.setFontItalic(true);

    _stringFmt.setForeground(Theme::kString);
    buildRules();
}

void CodeHighlighter::highlightBlock(const QString &text)
{
    for(const Rule&rule:_rules){
        auto it = rule.pattern.globalMatch(text);
        while(it.hasNext()){
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(),m.capturedLength(),rule.format);
        }
    }
    applyBlockComment(text);
}

void CodeHighlighter::buildRules()
{
    static const QStringList keywords = {
        "alignas", "alignof", "auto", "bool", "break", "case", "catch", "char",
        "class", "const", "constexpr", "continue", "decltype", "default", "delete",
        "do", "double", "else", "enum", "explicit", "extern", "false", "float",
        "for", "friend", "goto", "if", "inline", "int", "long", "mutable",
        "namespace", "new", "noexcept", "nullptr", "operator", "override",
        "private", "protected", "public", "return", "short", "signed", "sizeof",
        "static", "static_cast", "struct", "switch", "template", "this", "throw",
        "true", "try", "typedef", "typename", "union", "unsigned", "using",
        "virtual", "void", "volatile", "while", "final",
        "emit", "signals", "slots", "Q_OBJECT", "Q_UNUSED", "foreach",
    };

    QTextCharFormat keywordFmt;
    keywordFmt.setForeground(Theme::kKeyword);
    keywordFmt.setFontWeight(QFont::DemiBold);

    QTextCharFormat typeFmt;
    typeFmt.setForeground(Theme::kType);

    QTextCharFormat numberFmt;
    numberFmt.setForeground(Theme::kNumber);

    QTextCharFormat funcFmt;
    funcFmt.setForeground(Theme::kFunction);

    QTextCharFormat preprocFmt;
    preprocFmt.setForeground(Theme::kPreproc);

    // ── 顺序很重要：后 add 的规则会覆盖先 add 的规则在同一字符上的颜色。
    //   所以「字符串」和「注释」必须放在最后。
    _rules.append({ QRegularExpression(R"(^\s*#\s*\w+)", QRegularExpression::MultilineOption),
    preprocFmt });
    _rules.append({ QRegularExpression(R"(\bQ[A-Z]\w*\b)"), typeFmt });
    _rules.append({ QRegularExpression("\\b(" + keywords.join('|') + ")\\b"), keywordFmt });
    _rules.append({ QRegularExpression(R"(\b(0[xX][0-9a-fA-F]+|\d+\.?\d*[fFuUlL]*)\b)"),
                   numberFmt });
    _rules.append({ QRegularExpression(R"(\b[A-Za-z_]\w*(?=\s*\())"), funcFmt });
    QTextCharFormat stringFmt;
    stringFmt.setForeground(Theme::kString);
    _rules.append({ QRegularExpression(R"("([^"\\]|\\.)*")"), stringFmt });
    _rules.append({ QRegularExpression(R"('([^'\\]|\\.)*')"), stringFmt });

    _rules.append({ QRegularExpression(R"(//[^\r\n]*)"), _commentFmt });
    // _rules.append({
    //     QRegularExpression(R"(/\*[\s\S]*?\*/)",
    //                        QRegularExpression::DotMatchesEverythingOption),
    //     _commentFmt
    // });
}

void CodeHighlighter::applyBlockComment(const QString &text)
{
    setCurrentBlockState(Normal);

    int pos = 0;
    if (previousBlockState() == InBlockComment){
        const int end = text.indexOf("*/");
        if (end < 0){
            setFormat(0,text.length(),_commentFmt);
            setCurrentBlockState(InBlockComment);
            return;
        }
        setFormat(0,end+2,_commentFmt);
        pos = end+2;
    }

    while(true){
        const int begin =text.indexOf("/*",pos);
        if (begin < 0 ){
            break;
        }

        const int end = text.indexOf("*/",begin+2);
        if (end < 0 ){
            setFormat(begin, text.length() - begin, _commentFmt);
            setCurrentBlockState(InBlockComment);
            break;
        }
        setFormat(begin, end + 2 - begin, _commentFmt);
        pos = end + 2;
    }
}
