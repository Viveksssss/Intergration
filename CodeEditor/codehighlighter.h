#ifndef CODEHIGHLIGHTER_H
#define CODEHIGHLIGHTER_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
class QTextDocument;
class CodeHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit CodeHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString&text) override;
    enum State {
        Normal         = 0,
        InBlockComment = 1,
    };
private:
    struct Rule{
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    void buildRules();
    void applyBlockComment(const QString &text);

    QVector<Rule>_rules;
    QTextCharFormat _commentFmt;
    QTextCharFormat _stringFmt;
};

#endif // CODEHIGHLIGHTER_H
