#include "ShotPage.h"

#include "ShotCapture.h"
#include "SnippingOverlay.h"
#include "ShotTheme.h"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

ShotPage::ShotPage(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("截图标注工具"));
    setMinimumSize(880, 640);
    setFont(ShotTheme::uiFont(10));

    auto *title = new QLabel(QStringLiteral("截图标注"), this);
    QFont tf = ShotTheme::uiFont(20, true);
    title->setFont(tf);

    auto *desc = new QLabel(
        QStringLiteral("框选区域后可以直接画矩形、箭头、画笔、文字和马赛克。\n"
                       "Enter 复制到剪贴板，Ctrl+S 存成文件，Esc 取消。"),
        this);
    desc->setWordWrap(true);
    desc->setStyleSheet(QStringLiteral("color:#9a9aa8;"));

    m_shotBtn = new QPushButton(QStringLiteral("开始截图    Ctrl+Shift+A"), this);
    m_shotBtn->setCursor(Qt::PointingHandCursor);
    m_shotBtn->setMinimumHeight(44);
    m_shotBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background:#2ea8ff; color:#0b0b10; border:none;"
        "  border-radius:8px; font-weight:600; font-size:14px;"
        "}"
        "QPushButton:hover { background:#4fb8ff; }"
        "QPushButton:pressed { background:#1f8fe0; }"));
    connect(m_shotBtn, &QPushButton::clicked, this, &ShotPage::takeShot);

    m_status = new QLabel(QStringLiteral("还没有截图"), this);
    m_status->setStyleSheet(QStringLiteral("color:#9a9aa8;"));
    m_status->setAlignment(Qt::AlignCenter);

    m_preview = new QLabel(this);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumHeight(240);
    m_preview->setStyleSheet(QStringLiteral(
        "border:1px solid #2c2c36; border-radius:8px; color:#5a5a68;"));

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(40, 36, 40, 32);
    lay->setSpacing(14);
    lay->addWidget(title);
    lay->addWidget(desc);
    lay->addSpacing(6);
    lay->addWidget(m_shotBtn);
    lay->addWidget(m_status);
    lay->addWidget(m_preview, 1);

    // 全应用级快捷键：窗口不活跃也能用
    auto *sc = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")), this);
    sc->setContext(Qt::ApplicationShortcut);
    connect(sc, &QShortcut::activated, this, &ShotPage::takeShot);

    m_shotBtn->setFocus();
}

void ShotPage::takeShot()
{
    emit shotStarted();

    QWidget *topLevel = window();
    topLevel->hide();

    // 关键的一步：等这一帧真的画完、窗口真的从屏幕上消失再抓屏。
    // 0ms 的 singleShot 只是"下一轮事件循环"，这时窗口可能还没被合成器收走。
    QTimer::singleShot(220, this, [this, topLevel] {
        const auto restoreWindow = [this, topLevel] {
            if (topLevel && topLevel != this) {
                topLevel->show();
                topLevel->raise();
                topLevel->activateWindow();
            } else {
                show();
                raise();
                activateWindow();
            }
        };

        const QImage shot = ShotCapture::grabAll();
        if (shot.isNull()) {
            m_status->setText(QStringLiteral("抓屏失败：拿不到屏幕内容"));
            restoreWindow();
            return;
        }

        auto *overlay = new SnippingOverlay(shot);

        connect(overlay, &SnippingOverlay::finished,
                this, [this, restoreWindow](const QImage &img) {
            QGuiApplication::clipboard()->setImage(img);
            presentResult(img);
            restoreWindow();
        });

        connect(overlay, &SnippingOverlay::cancelled, this, [restoreWindow] {
            restoreWindow();
        });

        overlay->show();
        overlay->raise();
        overlay->activateWindow();
    });
}

void ShotPage::presentResult(const QImage &img)
{
    // QLabel 里放一张会被缩放的大图：先按标签宽度缩好，避免撑爆布局
    const int maxW = qMax(200, m_preview->width() - 16);
    const int maxH = qMax(160, m_preview->height() - 16);

    QPixmap pm = QPixmap::fromImage(img);
    pm = pm.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_preview->setPixmap(pm);

    m_status->setText(QStringLiteral("已复制到剪贴板 · %1 × %2 像素")
                          .arg(img.width())
                          .arg(img.height()));
}

void ShotPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0.0, QColor("#191921"));
    g.setColorAt(1.0, QColor("#101015"));
    p.fillRect(rect(), g);
}
