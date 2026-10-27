#include "codeeditor.h"
#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QPlainTextEdit>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CodeEditor"));
    app.setOrganizationName(QStringLiteral("Vivek"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.ico")));
    Theme::apply(app);

    MainWindow window;
    window.show();

    // 命令行参数统一走「标签页」，不再各个分支里手动 setPlainText
    const QStringList args = app.arguments();
    if (args.size() == 2){
        window.openPath(args.at(1));
    }else{
        const int openIdx = args.indexOf(QStringLiteral("--open"));
        if (openIdx >= 0 && openIdx + 1 < args.size()) {
            window.openPath(args.at(openIdx + 1));
        }
    }

    return app.exec();


}
