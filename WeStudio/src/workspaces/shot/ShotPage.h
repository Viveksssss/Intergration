#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

// ===========================================================================
//  外壳窗口：它本身没什么技术含量，只负责
//    · 提供一个"开始截图"的入口
//    · 截图前先把自己藏起来（否则会把自己抓进图里）
//    · 拿到结果后复制到剪贴板并显示预览
//  真正的活都在 SnippingOverlay 里。
// ===========================================================================
class ShotPage : public QWidget
{
    Q_OBJECT

public:
    explicit ShotPage(QWidget *parent = nullptr);

signals:
    void shotStarted();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void takeShot();
    void presentResult(const QImage &img);

    QPushButton *m_shotBtn = nullptr;
    QLabel      *m_preview = nullptr;
    QLabel      *m_status  = nullptr;
};
