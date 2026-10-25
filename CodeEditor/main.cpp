#include "codeeditor.h"
#include "mainwindow.h"

#include <QApplication>
#include <QPlainTextEdit>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const char* kDemoSource = R"SRC(// ------------------------------------------------------------------
//  演示文件：这块内容会被 CodeHighlighter 着色、被 FoldManager 扫描
//  点行号左边的三角可以折叠；光标贴着括号会高亮配对的另一个
// ------------------------------------------------------------------
#include <QApplication>
#include <QPlainTextEdit>
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
    editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");editor.setPlainText("hello { world }");
    editor.show();

    return app.exec();
}
)SRC";

    CodeEditor editor;
    editor.setPlainText(QString::fromUtf8(kDemoSource));
    editor.resize(1080,720);
    editor.setWordWrapEnabled(true);
    editor.show();

    return app.exec();
}
