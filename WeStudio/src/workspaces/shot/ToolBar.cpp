#include "ToolBar.h"

#include "ShotTheme.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QPainter>
#include <QSignalBlocker>
#include <QToolButton>

namespace {

constexpr int kBtnSize   = 30;
constexpr int kIconSize  = 18;
constexpr int kRadius    = 10;

// 一条 1px 竖线做分组分隔
QWidget *makeSeparator()
{
    auto *w = new QWidget;
    w->setFixedSize(1, 18);
    w->setStyleSheet(QStringLiteral("background:#3c3c48;"));
    return w;
}

}  // namespace

ToolBar::ToolBar(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setCursor(Qt::ArrowCursor);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(2);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    // ---- 绘图工具（互斥）
    struct ToolDef { Tool tool; QString tip; };
    const ToolDef tools[] = {
        { Tool::Rect,   QStringLiteral("矩形框  R")   },
        { Tool::Arrow,  QStringLiteral("箭头    A")   },
        { Tool::Pen,    QStringLiteral("画笔    P")   },
        { Tool::Text,   QStringLiteral("文字    T")   },
        { Tool::Mosaic, QStringLiteral("马赛克  M")   },
    };
    for (const ToolDef &d : tools)
        lay->addWidget(addToolButton(d.tool, d.tip));

    lay->addWidget(makeSeparator());

    // ---- 编辑动作
    m_undo = addActionButton("undo", QStringLiteral("撤销  Ctrl+Z"));
    lay->addWidget(m_undo);
    connect(m_undo, &QToolButton::clicked, this, &ToolBar::undoRequested);

    auto *clearBtn = addActionButton("trash", QStringLiteral("清空标注"));
    lay->addWidget(clearBtn);
    connect(clearBtn, &QToolButton::clicked, this, &ToolBar::clearRequested);

    lay->addWidget(makeSeparator());

    // ---- 收尾
    auto *copyBtn = addActionButton("copy", QStringLiteral("复制到剪贴板  Enter"));
    lay->addWidget(copyBtn);
    connect(copyBtn, &QToolButton::clicked, this, &ToolBar::copyRequested);

    auto *saveBtn = addActionButton("save", QStringLiteral("保存为文件  Ctrl+S"));
    lay->addWidget(saveBtn);
    connect(saveBtn, &QToolButton::clicked, this, &ToolBar::saveRequested);

    auto *closeBtn = addActionButton("close", QStringLiteral("取消  Esc"));
    lay->addWidget(closeBtn);
    connect(closeBtn, &QToolButton::clicked, this, &ToolBar::cancelRequested);

    setUndoEnabled(false);
    setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  border: none;"
        "  border-radius: 6px;"
        "  background: transparent;"
        "}"
        "QToolButton:hover { background: #35353f; }"
        "QToolButton:checked { background: #2ea8ff; }"
        "QToolTip { color: #e8e8f0; background: #22222b;"
        "           border: 1px solid #4a4a5a; padding: 3px; }"));

    adjustSize();
}

QToolButton *ToolBar::addToolButton(Tool t, const QString &tip)
{
    auto *b = new QToolButton(this);
    b->setCheckable(true);
    b->setAutoRaise(true);
    b->setFixedSize(kBtnSize, kBtnSize);
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setIcon(ShotTheme::toolIcon(t, kIconSize));
    b->setToolTip(tip);
    b->setCursor(Qt::ArrowCursor);

    m_group->addButton(b);
    m_toolButtons.append(b);

    // 用 lambda 捕获 t，不用 QButtonGroup 的 id 映射，少一层心智负担
    connect(b, &QToolButton::clicked, this, [this, b, t] {
        // 再点一次已经选中的按钮 = 取消工具，回到"调整选区"模式
        if (!b->isChecked()) {
            emit toolChanged(Tool::None);
        } else {
            emit toolChanged(t);
        }
    });
    return b;
}

QToolButton *ToolBar::addActionButton(const char *icon, const QString &tip)
{
    auto *b = new QToolButton(this);
    b->setAutoRaise(true);
    b->setFixedSize(kBtnSize, kBtnSize);
    b->setIconSize(QSize(kIconSize, kIconSize));
    b->setIcon(ShotTheme::actionIcon(icon, kIconSize));
    b->setToolTip(tip);
    b->setCursor(Qt::ArrowCursor);
    return b;
}

void ToolBar::setCurrentTool(Tool t)
{
    // 外部（比如快捷键 R/A/P/T/M）切工具时，按钮状态要跟着走
    m_group->setExclusive(false);
    for (QToolButton *b : m_toolButtons)
        b->setChecked(false);
    m_group->setExclusive(true);

    if (t == Tool::None)
        return;

    const Tool order[] = { Tool::Rect, Tool::Arrow, Tool::Pen, Tool::Text, Tool::Mosaic };
    for (int i = 0; i < int(sizeof(order) / sizeof(order[0])); ++i) {
        if (order[i] == t && i < m_toolButtons.size()) {
            QSignalBlocker block(m_toolButtons[i]);
            m_toolButtons[i]->setChecked(true);
            break;
        }
    }
}

void ToolBar::setUndoEnabled(bool on)
{
    if (m_undo)
        m_undo->setEnabled(on);
}

void ToolBar::placeNear(const QRect &sel, const QRect &bounds)
{
    adjustSize();

    const int gap = 10;
    int y = sel.bottom() + gap;
    if (y + height() > bounds.bottom() - 4)
        y = sel.top() - height() - gap;          // 下方放不下 → 上方
    if (y < bounds.top() + 4)
        y = bounds.top() + 4;                    // 上下都放不下（选区占满屏）

    int x = sel.center().x() - width() / 2;
    x = qBound(bounds.left() + 4, x, bounds.right() - width() - 4);

    move(x, y);
}

void ToolBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(ShotTheme::kPanelBorder, 1));
    p.setBrush(ShotTheme::kPanelBg);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), kRadius, kRadius);
}
