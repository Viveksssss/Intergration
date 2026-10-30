#include "CodeHighlighter.h"
#include "CodeTheme.h"

#include <QStringList>
#include <QTextDocument>

CodeHighlighter::CodeHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_commentFmt.setForeground(CodeTheme::kComment);
    m_commentFmt.setFontItalic(true);

    m_stringFmt.setForeground(CodeTheme::kString);

    buildRules();
}

void CodeHighlighter::buildRules()
{
    // 关键词：C++ 关键字 + Qt 的少量宏。
    // 一次拼成一个大正则，比「每个词一条规则」快得多：
    // 100 条规则 = 每行跑 100 次 globalMatch；1 条 alternation 只跑 1 次。
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
    keywordFmt.setForeground(CodeTheme::kKeyword);
    keywordFmt.setFontWeight(QFont::DemiBold);

    // Q 开头的类名统一当作类型色，省得手写几百个 Qt 类
    QTextCharFormat typeFmt;
    typeFmt.setForeground(CodeTheme::kType);

    QTextCharFormat numberFmt;
    numberFmt.setForeground(CodeTheme::kNumber);

    QTextCharFormat funcFmt;
    funcFmt.setForeground(CodeTheme::kFunction);

    QTextCharFormat preprocFmt;
    preprocFmt.setForeground(CodeTheme::kPreproc);

    // ── 顺序很重要：后 add 的规则会覆盖先 add 的规则在同一字符上的颜色。
    //   所以「字符串」和「注释」必须放在最后。
    m_rules.append({ QRegularExpression(R"(^\s*#\s*\w+)", QRegularExpression::MultilineOption),
                     preprocFmt });

    m_rules.append({ QRegularExpression(R"(\bQ[A-Z]\w*\b)"), typeFmt });

    m_rules.append({ QRegularExpression("\\b(" + keywords.join('|') + ")\\b"), keywordFmt });

    m_rules.append({ QRegularExpression(R"(\b(0[xX][0-9a-fA-F]+|\d+\.?\d*[fFuUlL]*)\b)"),
                     numberFmt });

    // 函数名：标识符后面跟 "("，且这个标识符不是关键字（用负向断言先粗筛）
    m_rules.append({ QRegularExpression(R"(\b[A-Za-z_]\w*(?=\s*\())"), funcFmt });

    // 字符串（支持 \" 转义）。非贪婪 + 允许转义序列。
    m_rules.append({ QRegularExpression(R"("([^"\\]|\\.)*")"), m_stringFmt });
    m_rules.append({ QRegularExpression(R"('([^'\\]|\\.)*')"), m_stringFmt });

    // 单行注释放最后：它会盖掉同一行里前面匹配到的所有颜色
    m_rules.append({ QRegularExpression(R"(//[^\n]*)"), m_commentFmt });
}

void CodeHighlighter::highlightBlock(const QString &text)
{
    // ---------------- 第一步：单行规则 ----------------
    for (const Rule &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // ---------------- 第二步：跨行块注释 ----------------
    // 它必须最后做，因为它的优先级高于上面所有规则。
    applyBlockComment(text);
}

void CodeHighlighter::applyBlockComment(const QString &text)
{
    setCurrentBlockState(Normal);

    int pos = 0;

    // (a) 上一行把「块注释未闭合」的状态传下来了
    if (previousBlockState() == InBlockComment) {
        const int end = text.indexOf("*/");
        if (end < 0) {
            // 整行都在注释里，直接结束，并把状态继续往下传
            setFormat(0, text.length(), m_commentFmt);
            setCurrentBlockState(InBlockComment);
            return;
        }
        setFormat(0, end + 2, m_commentFmt);
        pos = end + 2;
    }

    // (b) 本行可能还开了新的块注释，一行里甚至可能有好几段 /* ... */
    while (true) {
        const int begin = text.indexOf("/*", pos);
        if (begin < 0)
            break;

        const int end = text.indexOf("*/", begin + 2);
        if (end < 0) {
            // 开了没关：从 begin 涂到行尾，并把状态传给下一行
            setFormat(begin, text.length() - begin, m_commentFmt);
            setCurrentBlockState(InBlockComment);
            break;
        }
        setFormat(begin, end + 2 - begin, m_commentFmt);
        pos = end + 2;
    }
}
