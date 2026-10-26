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

    const QStringList args = app.arguments();
    if (args.size() == 2){
        const QString path = args.at(1);
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            window.editor()->setPlainText(QString::fromUtf8(file.readAll()));
            window.editor()->document()->setModified(false);
        }
    }else{
        const int openIdx = args.indexOf(QStringLiteral("--open"));
        if (openIdx >= 0 && openIdx + 1 < args.size()) {
            const QString path = args.at(openIdx + 1);
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                window.editor()->setPlainText(QString::fromUtf8(file.readAll()));
                window.editor()->document()->setModified(false);
            }
        }
    }

    return app.exec();


}
