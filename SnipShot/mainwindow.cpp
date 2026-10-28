#include "mainwindow.h"

#include "shotcapture.h"
#include "snippingoverlay.h"
#include "theme.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("SnipShot"));
    setWindowIcon(Theme::appIcon());
    setupTrayIcon();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupTrayIcon()
{
    auto *menu = new QMenu(this);

    m_regionCaptureAction = menu->addAction(
        Theme::appIcon(20), QStringLiteral("截图"),
        this, &MainWindow::requestRegionCapture);

    auto *fullScreenAction = menu->addAction(
        style()->standardIcon(QStyle::SP_ComputerIcon),
        QStringLiteral("全屏截图"),
        this, &MainWindow::requestFullScreenCapture);
    fullScreenAction->setToolTip(QStringLiteral("截取主屏幕并复制到剪贴板"));

    menu->addSeparator();

    menu->addAction(
        style()->standardIcon(QStyle::SP_DirOpenIcon),
        QStringLiteral("打开截图文件夹"),
        this, &MainWindow::openScreenshotFolder);

    menu->addSeparator();

    menu->addAction(
        style()->standardIcon(QStyle::SP_MessageBoxInformation),
        QStringLiteral("关于 SnipShot"),
        this, &MainWindow::showAboutDialog);

    menu->addAction(
        style()->standardIcon(QStyle::SP_DialogCloseButton),
        QStringLiteral("退出"),
        qApp, &QApplication::quit);

    m_trayIcon = new QSystemTrayIcon(Theme::appIcon(), this);
    m_trayIcon->setToolTip(QStringLiteral("SnipShot\n单击截图，右键打开菜单"));
    m_trayIcon->setContextMenu(menu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick) {
                    requestRegionCapture();
                }
            });

    m_trayIcon->show();
}

void MainWindow::requestRegionCapture()
{
    if (m_overlay) {
        m_overlay->show();
        m_overlay->raise();
        m_overlay->activateWindow();
        return;
    }
    if (m_captureScheduled)
        return;

    m_captureScheduled = true;
    QTimer::singleShot(220, this, [this] {
        m_captureScheduled = false;
        performRegionCapture();
    });
}

void MainWindow::requestFullScreenCapture()
{
    if (m_overlay || m_captureScheduled)
        return;

    m_captureScheduled = true;
    QTimer::singleShot(220, this, [this] {
        m_captureScheduled = false;
        performFullScreenCapture();
    });
}

void MainWindow::performRegionCapture()
{
    if (m_overlay)
        return;

    const QImage shot = ShotCapture::grabAll();
    if (shot.isNull()) {
        showTrayMessage(QStringLiteral("截图失败"),
                        QStringLiteral("无法读取当前屏幕，请稍后重试。"), true);
        return;
    }

    auto *overlay = new SnippingOverlay(shot);
    m_overlay = overlay;
    m_regionCaptureAction->setEnabled(false);

    connect(overlay, &SnippingOverlay::finished, this,
            [this](const QImage &image) {
                copyToClipboard(image);
                showTrayMessage(QStringLiteral("截图完成"),
                                QStringLiteral("图片已复制到剪贴板。"));
            });

    connect(overlay, &QObject::destroyed, this, [this] {
        m_regionCaptureAction->setEnabled(true);
    });

    overlay->show();
    overlay->raise();
    overlay->activateWindow();
}

void MainWindow::performFullScreenCapture()
{
    const QImage image = ShotCapture::grabAll();
    if (image.isNull()) {
        showTrayMessage(QStringLiteral("截图失败"),
                        QStringLiteral("无法读取当前屏幕，请稍后重试。"), true);
        return;
    }

    copyToClipboard(image);
    showTrayMessage(QStringLiteral("全屏截图完成"),
                    QStringLiteral("图片已复制到剪贴板。"));
}

void MainWindow::copyToClipboard(const QImage &image)
{
    if (!image.isNull())
        QGuiApplication::clipboard()->setImage(image);
}

void MainWindow::openScreenshotFolder()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (folder.isEmpty())
        folder = QDir::homePath();

    QDir().mkpath(folder);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folder))) {
        showTrayMessage(QStringLiteral("无法打开文件夹"),
                        QStringLiteral("请手动打开：\n%1").arg(folder), true);
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(
        nullptr,
        QStringLiteral("关于 SnipShot"),
        QStringLiteral(
            "<h3>SnipShot</h3>"
            "<p>轻量截图与标注工具</p>"
            "<p>单击托盘图标开始截图，右键查看更多操作。</p>"
            "<p>快捷键：R 矩形，A 箭头，P 画笔，T 文字，"
            "M 马赛克；Enter 复制，Ctrl+S 保存，Esc 取消。</p>"));
}

void MainWindow::showTrayMessage(const QString &title, const QString &message,
                                 bool warning)
{
    if (!m_trayIcon || !QSystemTrayIcon::supportsMessages())
        return;

    m_trayIcon->showMessage(
        title, message,
        warning ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information,
        1800);
}
