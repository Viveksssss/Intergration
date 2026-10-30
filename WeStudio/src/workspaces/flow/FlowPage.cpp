#include "FlowPage.h"
#include "NodeScene.h"
#include "NodeView.h"
#include "FlowTheme.h"

#include <QLabel>
#include <QStatusBar>
#include <QToolBar>

FlowPage::FlowPage(QWidget *parent)
    : QMainWindow(parent)
{
    m_scene = new NodeScene(this);
    m_view  = new NodeView(m_scene);
    setCentralWidget(m_view);

    buildToolBar();

    m_info = new QLabel;
    statusBar()->addPermanentWidget(m_info);

    // 图结构一变（连线/删节点/加节点）就刷新统计
    connect(m_scene, &NodeScene::graphChanged, this, &FlowPage::updateStatus);
    connect(m_view, &NodeView::zoomChanged, this, [this](double) { updateStatus(); });

    setWindowTitle(QStringLiteral("节点编辑器 Demo —— 拖端口连线 · 双击数值节点改值"));
    buildDemoGraph();
    updateStatus();
}

// ---------------------------------------------------------------- 工具栏
void FlowPage::buildToolBar()
{
    auto *tb = addToolBar(QStringLiteral("主工具栏"));
    tb->setMovable(false);

    // 三个"加节点"按钮。每个都用 addAction 生成 QToolButton，
    // 这样 FlowTheme 里的 QToolButton 样式表才能生效。
    QAction *a = tb->addAction(QStringLiteral("＋ 数值"));
    connect(a, &QAction::triggered, this, [this] { addNodeOfKind(NodeItem::Number); });

    a = tb->addAction(QStringLiteral("＋ 相加"));
    connect(a, &QAction::triggered, this, [this] { addNodeOfKind(NodeItem::Add); });

    a = tb->addAction(QStringLiteral("＋ 显示"));
    connect(a, &QAction::triggered, this, [this] { addNodeOfKind(NodeItem::Display); });

    tb->addSeparator();

    a = tb->addAction(QStringLiteral("删除选中"));
    connect(a, &QAction::triggered, this, [this] { m_scene->deleteSelected(); });

    a = tb->addAction(QStringLiteral("重建演示"));
    connect(a, &QAction::triggered, this, [this] {
        buildDemoGraph();
        m_view->resetZoom();
    });

    tb->addSeparator();

    auto *hint = new QLabel(
        QStringLiteral("  滚轮缩放 · 中键平移 · 拖端口连线 · 双击数值节点 +1  "));
    tb->addWidget(hint);
}

// ---------------------------------------------------------------- 演示图
void FlowPage::buildDemoGraph()
{
    m_scene->clearGraph();
    m_spawn = 0;

    NodeItem *a   = m_scene->addNode(NodeItem::Number,  QPointF(-350, -130));
    NodeItem *b   = m_scene->addNode(NodeItem::Number,  QPointF(-350,   30));
    NodeItem *sum = m_scene->addNode(NodeItem::Add,     QPointF( -60,  -50));
    NodeItem *out = m_scene->addNode(NodeItem::Display, QPointF( 330,  -50));

    a->setFixedValue(3.0);
    b->setFixedValue(4.0);

    m_scene->connectNodes(a, sum);      // 自动接到 sum 的第一个空输入
    m_scene->connectNodes(b, sum);      // 再接到第二个
    m_scene->connectNodes(sum, out);

    m_scene->refreshValues();
    m_scene->clearSelection();          // 加载/重建后不该残留任何选中态
    m_view->centerOn(0, -40);
}

// ---------------------------------------------------------------- 操作
QPointF FlowPage::viewCenter() const
{
    return m_view->mapToScene(m_view->viewport()->rect().center());
}

void FlowPage::addNodeOfKind(NodeItem::Kind kind)
{
    const QPointF base = viewCenter();

    // 连续点按钮时往右下错开 (26, 22)，而不是全部堆在正中
    const double dx = (m_spawn % 5) * 26.0;
    const double dy = (m_spawn % 5) * 22.0;
    ++m_spawn;

    // 减掉半个节点尺寸，让节点中心落在视图中心
    m_scene->addNode(kind, base + QPointF(dx - 84.0, dy - 50.0));
}

void FlowPage::updateStatus()
{
    m_info->setText(QStringLiteral("节点 %1    连线 %2    缩放 %3%")
                        .arg(m_scene->nodeCount())
                        .arg(m_scene->edgeCount())
                        .arg(int(qRound(m_view->zoom() * 100.0))));
}
