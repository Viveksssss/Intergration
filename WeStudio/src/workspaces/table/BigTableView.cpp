#include "BigTableView.h"
#include "BigModel.h"
#include "CellBackgroundDelegate.h"
#include "MultiLineDelegate.h"
#include "ProgressDelegate.h"
#include "StatusDelegate.h"
#include "TableTheme.h"

#include <QHeaderView>
#include <QPainter>
#include <QEvent>
#include <QFile>
#include <QTextStream>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>

BigTableView::BigTableView(QWidget *parent)
    : QTableView(parent)
{
    // -------------------------------------------------------------------
    //  虚拟化相关的开关
    //
    //  QTableView 默认就是虚拟化的:滚动条范围由 rowCount × rowHeight
    //  算出来,不用遍历真实行;paintEvent 只为可见行调 delegate。
    //  下面这些设置是在**保护**这个前提,不是打开它。
    // -------------------------------------------------------------------

    //  1. 行高固定。设了默认行高之后,QTableView 用"行号 × 行高"直接定位,
    //     O(1)。如果不设、又开了 wordWrap,Qt 会退回"从头累加算位置"——
    //     滚到第 90 万行就要累加 90 万次,而且每帧都算一次。
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(TableTheme::kDefaultRowH);

    //  2. 关掉自动换行。开了它,行高就必须逐行算,虚拟化立刻退化成 O(n)。
    //     多行文本由 delegate 自己排版(见 MultiLineDelegate),
    //     行高由该列的 sizeHint 给出——**不用 setWordWrap**。
    setWordWrap(false);

    //  3. 滚动模式用 PerPixel。PerItem 在百万行下,滚动条 value 直接是行号,
    //     精度是够的,但滚轮一格跳 3 行会显得很钝;PerPixel 更顺,且同样 O(1)。
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    //  4. 关掉"选中整行"以外的花哨效果。选中整行会让 delegate 收到
    //     State_Selected,这正是我们要的;但 hover 高亮要开
    //     setMouseTracking,会增加 mouseMove 事件量,百万行下不划算。
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    //  5. 不要自动 resize 列。resizeColumnsToContents() 会遍历**所有行**
    //     求最大宽度——100 万行 × 6 列,一次就是几百万次 data() 调用。
    //     这是百万行表最常见的卡死原因,而且它藏在 setModel 之后一行。
    horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    //  不要 stretchLastSection。开了它之后,最后一列会去吃掉"视口宽度 - 其他列",
    //  而视口宽度里包含竖直滚动条的 17px —— 最后一列的文字会紧贴滚动条甚至
    //  被裁掉(截图里"体积"列的 GB 就断在那)。固定列宽 + 留出余量更稳。
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setHighlightSections(false);

    setShowGrid(false);
    setAlternatingRowColors(false);    // 斑马纹由 delegate 自己画,统一控制
    setWordWrap(false);

    //  6. 统一底色。视口和表格都设,否则滚动时露出的空白区颜色不一致。
    //
    //  **必须显式关掉 QSS 的选中高亮**,否则会踩一个很隐蔽的坑:
    //  QTableView 遇到 QSS 时会走 QStyleSheetStyle,它把选中行的背景
    //  按 palette.Highlight 画——默认是浅蓝甚至白色,不管你有没有
    //  在 delegate 里自己画。表现就是"整行只有被选中那一行是白的",
    //  而且它在 delegate 画完之后**再盖上去**,你怎么调 delegate 都没用。
    //
    //  办法:把 QSS 里的 selection-background-color 设成 transparent,
    //  让选中高亮交给 delegate 自己画(TableTheme::kRowSelected)。
    //  同理把 alternate-background-color 也清掉,斑马纹统一由 delegate 负责。
    //
    //  还有 QTableCornerButton —— 左上角"行号列与表头交叉"的那个小方块。
    //  它是独立的一个子控件,不设它的背景,列宽不足视口宽度时它会露出来,
    //  默认是一块浅灰。这个色块在深色表里极其扎眼。
    setStyleSheet(QStringLiteral(
        "QTableView { background:%1; border:none;"
        "  selection-background-color:transparent;"
        "  selection-color:%2; }"
        "QHeaderView::section { background:%3; color:%4; border:none;"
        "  border-right:1px solid %5; padding-left:%6px; }"
        "QTableCornerButton::section { background:%3; border:none; }")
        .arg(TableTheme::kViewBg.name(),
             TableTheme::kText.name(),
             TableTheme::kHeaderBg.name(),
             TableTheme::kHeaderText.name(),
             TableTheme::kGridLine.name())
        .arg(TableTheme::kCellPadX));

    verticalHeader()->hide();          // 序号列已经有一列了,左侧行号没必要

    //  装 viewport 事件过滤器(见 eventFilter 的注释)。
    viewport()->installEventFilter(this);

    setFocusPolicy(Qt::StrongFocus);
    setFont(TableTheme::uiFont(9));
}

// ===========================================================================
//  setModel 的包装
//
//  为什么包一层:setModel 之后必须立刻装 delegate、设列宽。
//  这些动作有顺序依赖——列宽要在 delegate 装好之后再设,
//  否则最后一列的 stretch 行为和 sizeHint 对不上。
// ===========================================================================
void BigTableView::setBigModel(BigModel *m)
{
    m_model = m;
    setModel(m);
    installDelegatesForCurrentModel();

    //  首次装模型时还没有 resize 事件(窗口可能已在最终尺寸上),
    //  主动分配一次列宽。延后一轮,等 viewport 尺寸定下来。
    QTimer::singleShot(0, this, [this] { distributeColumnWidths(); });
}

// ===========================================================================
//  installDelegatesForCurrentModel
//
//  为什么单独拉出来一个函数:setModel() 之后(不管是装原模型还是装代理模型)
//  都要重做这一步。代理模型 view 的列数/列宽/delegate 全部不继承,
//  这是 QSortFilterProxyModel 最容易漏的一步——漏了之后表格看起来
//  "什么都能显示,就是样式全没了",很像 QSS 没生效,其实是 delegate 丢了。
// ===========================================================================
void BigTableView::installDelegatesForCurrentModel()
{
    //  列宽。数字列窄,文本列宽。不用 resizeColumnsToContents。
    //
    //  宽度之和故意**小于**窗口宽度(80+240+150+110+420+130 = 1130,
    //  窗口 1360)。留出的空档由 QTableView 的底色填,比让最后一列
    //  被滚动条挤窄要好看。如果你就是想让表格铺满,设
    //  sectionResizeMode(Fixed) + 在 resizeEvent 里按比例分配,
    //  也不要开 stretchLastSection(理由见构造函数里的注释)。
    setColumnWidth(BigModel::ColId,       80);
    setColumnWidth(BigModel::ColName,    240);
    setColumnWidth(BigModel::ColProgress,150);
    setColumnWidth(BigModel::ColStatus,  110);
    setColumnWidth(BigModel::ColDesc,    420);
    setColumnWidth(BigModel::ColSize,    130);

    installDelegates();
}

// ===========================================================================
//  按列装 delegate
//
//  setItemDelegateForColumn(列号, d) —— 只影响这一列。
//  没有指定 delegate 的列,用 QAbstractItemView 自带的默认 delegate,
//  它会画背景 + 文字,够用。不要为了"统一风格"给每一列都塞一个自写 delegate,
//  那等于把一个本来能跑的系统全部接管,维护成本翻几倍。
// ===========================================================================
void BigTableView::installDelegates()
{
    auto *prog = new ProgressDelegate(this);
    auto *multi = new MultiLineDelegate(2, this);      // 最多两行
    auto *status = new StatusDelegate(this);

    setItemDelegateForColumn(BigModel::ColProgress, prog);
    setItemDelegateForColumn(BigModel::ColStatus,   status);
    setItemDelegateForColumn(BigModel::ColDesc,     multi);

    // -------------------------------------------------------------------
    //  剩下的列(序号/任务/体积)不想要自绘,但**也不能用 Qt 的默认 delegate**。
    //
    //  默认 delegate 只画"文字 + 它自己理解的背景",不画斑马纹。
    //  结果就是这三格是纯底色,而进度/状态/说明三格有斑马纹 ——
    //  整行颜色从第 3 列开始断开,截图里非常明显。
    //
    //  办法:套一个 CellBackgroundDelegate。它画完背景就把文字交给基类,
    //  自己什么都不用实现 —— 正好是"要统一背景、不要自定义内容"的场景。
    //
    //  这一条也是**全局 setItemDelegate** 存在的意义:
    //  它给"没有单独指定 delegate 的列"提供一个默认值。
    //  先按列设特例,再设全局默认,顺序符合"先特例后兜底"的直觉。
    // -------------------------------------------------------------------
    setItemDelegate(new CellBackgroundDelegate(this));

    //  描述列给大一点行高,两行文字才放得下。
    //  注意:这行的意思是"物理行高固定 62px",不是"按内容算"。
    //  固定行高 + delegate 自己节流(maxLines=2),才能同时得到
    //  "多行文本" 和 "O(1) 定位"。
    verticalHeader()->setDefaultSectionSize(62);
}

// ===========================================================================
//  paintEvent:什么都不做,只把活交给基类
//
//  **这里有一个坑,而且是我第一次实现时踩到的**:
//  重载 QTableView::paintEvent 数不到"单元格绘制次数"。
//
//  QTableView 的画法是:
//    · QTableView::paintEvent  → 调基类 QAbstractItemView::paintEvent
//    · QAbstractItemView::paintEvent 里其实只画了"视口背景"
//    · 真正逐格调 delegate->paint() 的是 QTableViewPrivate::drawContents(),
//      它由 viewport 的 paint 事件驱动(QAbstractScrollArea::viewportEvent)
//
//  所以重载 QTableView::paintEvent 只能数到"背景刷了几次",
//  而且实测**连一次都数不到**——因为这个函数根本不在 viewport 的
//  绘制路径上(trace 里 paint=0 就是这个原因)。
//
//  正确做法:给 viewport() 装事件过滤器,拦 QEvent::Paint。
//  这才是"一帧"的真实来源。
//
//  ★ 但踩完这个坑之后我又踩了第二个:
//  我"顺手保留"了这个重载里的 ++m_paintCalls,想着"多算一点无所谓"。
//  结果是 paint 数**翻倍**——s7 截图里的"单帧 paint 6 帧"就是这么来的
//  (真实是 3 帧)。计数器的价值全在数字准不准,多算一倍等于没算。
//
//  所以这个重载现在**只做转发**。它甚至可以不写,留着是因为要从
//  编译期提醒自己:QTableView 的绘制不在这个函数里。
// ===========================================================================
void BigTableView::paintEvent(QPaintEvent *e)
{
    QTableView::paintEvent(e);
}

bool BigTableView::eventFilter(QObject *watched, QEvent *ev)
{
    if (watched == viewport() && ev->type() == QEvent::Paint) {
        ++m_paintCalls;
        //  每 8 帧通知一次"统计该刷新了"。为什么不在每帧都发信号:
        //  updateStatus 会读 dataCalls() 并重设 QLabel 文本,
        //  QLabel 文本变化又会触发一次重绘 —— 每帧都发就变成
        //  "重绘 → 改文本 → 再重绘"的自激循环。
        //  8 帧一次(约 130ms)对状态条这种展示性数字足够了。
        if ((m_paintCalls % 8) == 0)
            emit statsChanged();
    }
    return QTableView::eventFilter(watched, ev);
}

// ===========================================================================
//  sizeHintForRow —— **不要在这里返回"固定行高"**
//
//  这一段的注释是这张表最值得读的一段,因为它记着一个"看着完全正确、
//  实际慢了 7 万倍"的修复。
//
//  背景:我想数"视图问了多少次行高",于是重载了它。第一版写成:
//
//      int BigTableView::sizeHintForRow(int row) const {
//          return verticalHeader()->defaultSectionSize();   // 62
//      }
//
//  看起来很合理——行高确实是统一的 62px,直接给数不是最快吗?
//
//  实测:整个界面刚 show 出来,data() 被调了 6,000,573 次,
//  rowCount() 被调了 1,000,000 次。
//
//  原因:QTableView 判断"所有行是否等高"的方式,是拿 sizeHintForRow()
//  的返回值 **和基类实现做对比**。我的重载返回 62,而基类(通过
//  delegate 的 sizeHint)返回的可能是不带 padding 的 34 或者 62-某个值,
//  两者不等 → QTableView 认定"这个表的行高不统一" → 放弃 O(1) 的
//  "行号 × 行高"定位 → 退回**逐行累加**去算滚动条范围。
//  1,000,000 行 × 6 列,于是有了那 600 万。
//
//  而这个 bug 的可怕之处是:**画面完全正常**。滚动也还算流畅
//  (600 万次整数运算在现代 CPU 上是几十毫秒)。如果不是把调用数
//  数出来,它会一直藏在"能跑"的表象下面。
//
//  正确的做法(就是下面这两行):老实交给基类。
//  基类知道 delegate 的 sizeHint、知道 defaultSectionSize,
//  它会返回一个"和它自己判断逻辑一致"的值。
//  计数照样能加,只是不要自作聪明地改返回值。
//
//  顺带记下结论:**要统计,不要接管**。
//  重载虚函数来埋点时,一定要让原实现继续跑完,不要用自己的实现替掉它。
// ===========================================================================
int BigTableView::sizeHintForRow(int row) const
{
    ++m_sizeHintCalls;
    return QTableView::sizeHintForRow(row);
}

int BigTableView::visibleRowCount() const
{
    //  rowAt() 返回"这个 y 坐标属于第几行",越界返回 -1。
    //  用视口上下边界各取一次,差值就是可见行数(含部分露出的行)。
    const int top = rowAt(0);
    const int bot = rowAt(viewport()->height() - 1);
    if (top < 0)
        return 0;
    if (bot < 0)
        return model() ? model()->rowCount() - top : 0;
    return bot - top + 1;
}

void BigTableView::resetCounters()
{
    m_paintCalls    = 0;
    m_sizeHintCalls = 0;
}

// ===========================================================================
//  resizeEvent:把剩余宽度补给最后一列
//
//  为什么不用 stretchLastSection:
//    它在算"剩余宽度"时用的是 **表格控件宽度**,而我们真正想让表格铺满的是
//    **视口宽度**。两者差着竖直滚动条的宽度(约 17px)和边框。
//    结果就是最后一列比实际可用空间宽 17px,文字右边被裁掉——
//    截图里"1.8 GB"只剩"1.8 GI"就是这个。
//
//  手写这个逻辑只需要三步:
//    1. 其它列宽度之和
//    2. 视口宽度 - 之和 = 剩余
//    3. 剩余不小于最小宽度,就赋给最后一列
//
//  注意要防"剩余为负"(窗口被拖得很窄)。负数赋给列宽会让 Qt 内部断言失败。
// ===========================================================================
void BigTableView::resizeEvent(QResizeEvent *e)
{
    QTableView::resizeEvent(e);

    //  关键:**必须等基类处理完再算**,而且"基类处理完"不等于"viewport
    //  尺寸已经定下来"。QTableView 的 viewport 是子控件,它的 resize
    //  会在父控件 resizeEvent 之后由布局系统派发。
    //
    //  trace 里看到的 vpW=623 就是这一刻的中间值(还没算上滚动条和表头),
    //  用它算出的 last=343 是错的。真正稳定下来的宽度是 1343。
    //
    //  办法:把计算推到事件循环下一轮。用 QTimer::singleShot(0) 就够了,
    //  那时 viewport 的 resizeEvent 已经跑过,viewport()->width() 是真的。
    QTimer::singleShot(0, this, [this] { distributeColumnWidths(); });
}

// ===========================================================================
//  distributeColumnWidths:把剩余宽度补给最后一列
// ===========================================================================
void BigTableView::distributeColumnWidths()
{
    if (!model())
        return;

    const int last = model()->columnCount() - 1;
    if (last < 0)
        return;

    //  各列的"理想宽度"。这是 design 值,不是约束:
    //  窗口够宽就按它来,窗口不够宽就按比例缩。
    static const int kIdeal[kMaxCols] = { 80, 240, 150, 110, 420, 130 };

    //  每列的绝对下限。低于它就只剩省略号了,不如让用户横向滚动。
    //  最后一列(体积)的下限给 90 —— "1.8 GB" 正好放得下。
    static const int kMinW[kMaxCols] = { 60, 120, 120, 90, 160, 90 };

    const int n = model()->columnCount();
    if (n > kMaxCols)
        return;

    const int availVp = viewport()->width();

    //  ---- 第一步:算理想宽度之和 ----
    int idealSum = 0;
    for (int c = 0; c < n; ++c)
        idealSum += kIdeal[c];

    //  ---- 第二步:视口比理想值宽 -> 把富余全给最后一列 ----
    //
    //  这一步就是原先"用 stretchLastSection 结果被裁字"要解决的问题:
    //  stretchLastSection 算的是控件宽度(含 17px 滚动条),而这里算的是
    //  viewport 宽度 —— 两者的差正好是那 17px。
    if (availVp >= idealSum) {
        for (int c = 0; c < last; ++c)
            setColumnWidth(c, kIdeal[c]);
        setColumnWidth(last, availVp - (idealSum - kIdeal[last]));
        return;
    }

    //  ---- 第三步:视口比理想值窄 -> 按比例缩,但不缩到下限以下 ----
    //
    //  为什么必须做这一步:s8 的截图里体积列被裁掉,就是因为它只做了
    //  第二步。窗口一窄,availVp < idealSum,那段代码的 if 不成立,
    //  于是**前五列保持理想宽度不动、最后一列只能吃 130 的默认值**,
    //  总和超出视口 -> 横向挤出去了。
    //
    //  缩法是"超出多少、按可缩的量等比例分摊",而不是简单地
    //  width * ratio —— 后者会把本来就在下限的列也压穿。
    int minSum = 0;
    for (int c = 0; c < n; ++c)
        minSum += kMinW[c];

    if (availVp <= minSum) {
        //  已经比所有下限之和还窄:全部取下限,让横向滚动条出场。
        //  这是唯一正确的处置 —— 硬挤下去只会让每一列都变成省略号。
        for (int c = 0; c < n; ++c)
            setColumnWidth(c, kMinW[c]);
        return;
    }

    const int shrinkable = idealSum - minSum;   // 总共可以缩掉多少
    const int need       = idealSum - availVp;  // 需要缩掉多少
    int assigned = 0;
    for (int c = 0; c < n - 1; ++c) {
        const int room = kIdeal[c] - kMinW[c];
        const int cut  = shrinkable > 0 ? (need * room) / shrinkable : 0;
        const int w    = kIdeal[c] - cut;
        setColumnWidth(c, w);
        assigned += w;
    }
    //  最后一列吃掉剩下的零头,保证总和**恰好**等于视口宽度,
    //  不会因为整数除法的舍入在右边留一条 1~2px 的底色缝。
    setColumnWidth(last, qMax(kMinW[last], availVp - assigned));
}
