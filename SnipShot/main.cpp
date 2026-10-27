#include "mainwindow.h"
#include "snippingoverlay.h"

#include <QApplication>
#include <QLabel>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QPixmap>
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

    app.setFont(Theme::uiFont(10));

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

    const QImage shot = ShotCapture::grabAll();
    if (shot.isNull())
        return 1;

    auto *overlay = new SnippingOverlay(shot);
    overlay->show();
    overlay->raise();

    return app.exec();
}
