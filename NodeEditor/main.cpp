#include <QApplication>
#include <QTimer>

#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Theme::apply(app);

    MainWindow w;
    w.resize(1160, 720);
    w.show();

    // 自检用：NodeEditor.exe --shot out.png  → 延时截图然后退出
    const QStringList args = QCoreApplication::arguments();
    const int i = args.indexOf("--shot");
    if (i >= 0 && i + 1 < args.size()) {
        const QString out = args[i + 1];
        QTimer::singleShot(1500, &w, [&w, out] {
            w.grab().save(out);
            qApp->quit();
        });
    }

    return app.exec();
}
