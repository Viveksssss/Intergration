#include "TablePage.h"
#include "BigModel.h"
#include "BigTableView.h"
#include "TableTheme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QScrollBar>
#include <QFile>
#include <QTextStream>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QVBoxLayout>

// ===========================================================================
//  过滤用代理模型
//
//  这里刻意**不做排序**(不调 sort())。原因在手册 s6 里详细讲,
//  一句话版本:QSortFilterProxyModel 的过滤是"逐行问一遍",
//  100 万行就是 100 万次 filterAcceptsRow —— 每次都要算一次 row()。
//  在 100 万行上点一下过滤,会卡住 2~4 秒。
//
//  但它仍然值得展示:因为它把"代理模型这一层"的存在讲清楚了,
//  而且它的卡顿本身就是这一阶段要教的东西。
// ===========================================================================
namespace {

class FilterProxy : public QSortFilterProxyModel
{
public:
    explicit FilterProxy(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        //  无过滤时不要走 filterAcceptsRow。这一句能省掉 100 万次调用。
        //  很多人不知道 setFilterFixedString("") 仍然会逐行调,
        //  因为它要判断"空串算不算匹配"。显式关掉最稳。
        setDynamicSortFilter(false);
    }

    QString needle;

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (needle.isEmpty())
            return true;
        //  只在"任务名"这一列上匹配。不要为了"模糊搜全表"把 6 列都问一遍——
        //  那样每次过滤是 600 万次 data()。
        const QModelIndex idx = sourceModel()->index(row, BigModel::ColName, parent);
        return idx.data(Qt::DisplayRole).toString().contains(needle);
    }
};

}  // namespace

TablePage::TablePage(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
}

void TablePage::buildUi()
{
    setWindowTitle(QStringLiteral("百万行表格"));
    resize(1360, 780);

    auto *central = new QWidget(this);
    central->setStyleSheet(QStringLiteral(
        "background:%1;").arg(TableTheme::kWindowBg.name()));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 标题条 ----
    //  自绘一条,而不是 QToolBar。QToolBar 的样式在不同平台差异很大,
    //  想统一还得写 QSS;既然上面已经有 QSS 了,不如直接摆控件省事。
    auto *title = new QWidget;
    title->setFixedHeight(56);
    title->setStyleSheet(QStringLiteral("background:%1;").arg(TableTheme::kHeaderBg.name()));
    auto *tl = new QHBoxLayout(title);
    tl->setContentsMargins(20, 0, 20, 0);

    auto *h = new QLabel(QStringLiteral("百万行表格"));
    h->setFont(TableTheme::uiFont(12, true));
    h->setStyleSheet(QStringLiteral("color:%1;").arg(TableTheme::kText.name()));
    tl->addWidget(h);

    auto *sub = new QLabel(QStringLiteral(
        "QAbstractTableModel · 虚拟化 · 自定义 delegate"));
    sub->setFont(TableTheme::uiFont(9));
    sub->setStyleSheet(QStringLiteral("color:%1;").arg(TableTheme::kTextDim.name()));
    tl->addWidget(sub);
    tl->addStretch(1);

    m_filter = new QLineEdit;
    m_filter->setPlaceholderText(QStringLiteral("过滤任务名…"));
    m_filter->setFixedWidth(220);
    m_filter->setFont(TableTheme::uiFont(9));
    m_filter->setStyleSheet(QStringLiteral(
        "QLineEdit { background:%1; color:%2; border:1px solid %3;"
        "  border-radius:6px; padding:5px 9px; }"
        "QLineEdit:focus { border-color:%4; }")
        .arg(TableTheme::kViewBg.name(),
             TableTheme::kText.name(),
             TableTheme::kGridLine.name(),
             TableTheme::kAccent.name()));
    tl->addWidget(m_filter);

    root->addWidget(title);

    // ---- 表格 ----
    m_model = new BigModel(this);
    m_view  = new BigTableView;
    m_view->setBigModel(m_model);
    root->addWidget(m_view, 1);

    // ---- 状态条 ----
    m_stat = new QLabel;
    m_stat->setFont(TableTheme::monoFont(9));
    m_stat->setStyleSheet(QStringLiteral(
        "color:%1; background:%2; padding:6px 20px;")
        .arg(TableTheme::kTextDim.name(), TableTheme::kHeaderBg.name()));
    root->addWidget(m_stat);

    setCentralWidget(central);

    // ---- 连线 ----
    //  过滤:用户停止输入 300ms 后再算。不加这个延迟,
    //  每敲一个字符都要过 100 万行,输入框会卡成幻灯片。
    auto *debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(300);
    connect(m_filter, &QLineEdit::textChanged, debounce,
            [debounce] { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, [this] {
        const QString s = m_filter->text().trimmed();
        if (s.isEmpty()) {
            //  从代理切回真模型:直接把原模型装回去,
            //  比"设一个匹配全部的正则"快无数倍。
            //
            //  注意这里不做 qobject_cast<FilterProxy*>(view->model()) 判断:
            //  FilterProxy 在匿名命名空间里、没有 Q_OBJECT,
            //  qobject_cast 根本编不过(它要求 Q_OBJECT 宏)。
            m_view->setBigModel(m_model);
        } else {
            auto *proxy = new FilterProxy(m_view);
            proxy->needle = s;
            proxy->setSourceModel(m_model);
            //  代理的列宽不会从原模型继承。setModel 之后要重新装 delegate
            //  和列宽,否则整张表所有列都变成默认 100px、delegate 全丢。
            //  这是 QSortFilterProxyModel 最常踩的一个坑。
            m_view->setModel(proxy);
            m_view->installDelegatesForCurrentModel();
        }
        updateStatus();
    });

    //  滚动时更新状态条上的"可见行"数字——这是虚拟化最直观的证据:
    //  滚到任何位置,可见行永远是 10 几行。
    connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this] { updateStatus(); });

    //  首帧画完之后再刷一次,让 data() 计数显示成真数字。
    //  这个信号由 BigTableView 在每 8 帧发一次(见 eventFilter)。
    connect(m_view, &BigTableView::statsChanged, this, [this] { updateStatus(); });
}

void TablePage::setRowCount(int n)
{
    m_model->setRowCount(n);
    updateStatus();
}

QString TablePage::statusText() const
{
    return m_stat ? m_stat->text() : QString();
}

// ===========================================================================
//  updateStatus
//
//  这里读的是 **m_model**(原模型)的 dataCalls,不是 view->model()。
//  区别很重要:如果过滤时 view 装的是代理模型,data() 是代理在收,
//  原模型的计数就不会动。想让这个数字一直有代表性,就得在代理里
//  透传计数(手册 s6 讲了怎么做)。这里用的是原模型,所以过滤状态下
//  这个数字会停住——这是刻意的,它反映的是"底层数据被问了几次"。
// ===========================================================================
void TablePage::updateStatus()
{
    const int total = m_view->model() ? m_view->model()->rowCount() : 0;
    const int top   = m_view->verticalScrollBar()->value();
    const int vis   = m_view->visibleRowCount();

    m_stat->setText(QStringLiteral(
        "总行数 %1  ·  可见 %2 行  ·  滚动位置 %3 px  ·  data() 调用 %4 次  ·  paint %5 帧")
        .arg(QLocale(QLocale::Chinese).toString(total))
        .arg(vis)
        .arg(top)
        .arg(m_model->dataCalls())
        .arg(m_view->paintCalls()));
}
