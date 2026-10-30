#include "WeStudioWindow.h"

#include <QApplication>
#include <QColor>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFont>
#include <QPalette>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("WeStudio"));
    QApplication::setApplicationDisplayName(QStringLiteral("WeStudio"));
    QApplication::setOrganizationName(QStringLiteral("WeStudio"));

    app.setStyle(QStringLiteral("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#0d0f14")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#e7eaf2")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#151821")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#1c202a")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#e7eaf2")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#232936")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#e7eaf2")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#62d2a2")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#091017")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#232936")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#e7eaf2")));
    app.setPalette(palette);

    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(9);
    app.setFont(font);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("WeStudio desktop workspace"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption pageOption(
        QStringList() << QStringLiteral("p") << QStringLiteral("page"),
        QStringLiteral("Open workspace: board, flow, table, code, shot, timeline, collab."),
        QStringLiteral("name"),
        QStringLiteral("board"));
    QCommandLineOption shotOption(
        QStringList() << QStringLiteral("shot"),
        QStringLiteral("Save a window screenshot and exit (smoke test)."),
        QStringLiteral("path"));
    parser.addOption(pageOption);
    parser.addOption(shotOption);
    parser.process(app);

    WeStudioWindow window;
    if (!window.showPage(parser.value(pageOption))) {
        window.showPage(QStringLiteral("board"));
    }
    window.show();

    if (parser.isSet(shotOption)) {
        const QString path = parser.value(shotOption);
        QTimer::singleShot(900, &window, [&app, &window, path] {
            const bool ok = window.grab().save(path);
            app.exit(ok ? 0 : 2);
        });
    }

    return app.exec();
}
