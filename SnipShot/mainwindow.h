#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

class QAction;
class QImage;
class QSystemTrayIcon;
class SnippingOverlay;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupTrayIcon();
    void requestRegionCapture();
    void requestFullScreenCapture();
    void performRegionCapture();
    void performFullScreenCapture();
    void copyToClipboard(const QImage &image);
    void openScreenshotFolder();
    void showAboutDialog();
    void showTrayMessage(const QString &title, const QString &message,
                         bool warning = false);

    QSystemTrayIcon *m_trayIcon = nullptr;
    QAction *m_regionCaptureAction = nullptr;
    QPointer<SnippingOverlay> m_overlay;
    bool m_captureScheduled = false;
};
#endif // MAINWINDOW_H
