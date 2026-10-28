#include "mainwindow.h"
#include "snippingoverlay.h"

#include <QApplication>
#include <QMessageBox>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>

#include "shotcapture.h"
#include "theme.h"

// ===========================================================================
static void runSelfShot(const QString &outPath, bool useRealScreen)
{
    QTimer::singleShot(260, [outPath, useRealScreen] {
        const QImage shot = useRealScreen ? ShotCapture::grabAll()
                                          : ShotCapture::makeTestDesktop();
        if (shot.isNull()) {
            qApp->quit();
            return;
        }

        auto *overlay = new SnippingOverlay(shot);
        overlay->show();
        overlay->raise();

        QTimer::singleShot(420, [overlay, outPath] {
            overlay->grab().save(outPath);
            qApp->quit();
        });
    });
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("SnipShot"));
    app.setApplicationDisplayName(QStringLiteral("SnipShot"));
    app.setApplicationVersion(QStringLiteral("0.1"));
    app.setOrganizationName(QStringLiteral("SnipShot"));
    app.setFont(Theme::uiFont(10));
    app.setWindowIcon(Theme::appIcon(64));
    app.setQuitOnLastWindowClosed(false);

    QString shotPath;
    bool useRealScreen = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QLatin1String("--shot") && i + 1 < argc)
            shotPath = QString::fromLocal8Bit(argv[++i]);
        else if (a == QLatin1String("--real"))
            useRealScreen = true;
    }

    if (!shotPath.isEmpty()) {
        runSelfShot(shotPath, useRealScreen);
        return app.exec();
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(
            nullptr, QStringLiteral("SnipShot"),
            QStringLiteral("当前系统托盘不可用，SnipShot 无法启动。"));
        return 1;
    }

    MainWindow mainWindow;
    mainWindow.hide();
    return app.exec();
}
