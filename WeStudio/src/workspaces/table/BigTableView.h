#pragma once

#include <QTableView>

class BigModel;
class QSortFilterProxyModel;

// ===========================================================================
//  BigTableView —— 包一层 QTableView,做三件事:
//
//  1. **打开虚拟化需要的开关**,并把它写清楚(见构造函数里的注释)
//  2. **按列装 delegate**
//  3. **统计"到底画了多少格"**,用来证明虚拟化真的生效
//
//  第 3 点最容易被忽略。QTableView 天生就是虚拟化的——它只为可见区域
//  问数据。但只要你踩中下面任何一个坑,虚拟化会静默失效:
//    · setWordWrap(true) + resizeRowsToContents()
//    · 调用了 setVerticalScrollMode(QAbstractItemView::ScrollPerItem)
//    · 用 setIndexWidget 给每一行塞了控件
//    · 重载 sizeHintForRow 时在里面遍历所有行
//  这些坑**不会报错**,只会让滚到 50 万行时卡死。所以要能数。
//
//  ---- 但"能数"之后,要先弄清一件事:数出来的东西是谁造成的 ----
//
//  实测发现:同一个程序、同一份代码,累计 data() 调用量会在
//      几百   ←→   600 万(= rowCount × 列数)
//  之间跳,而且不是随机跳,是"一段时间一直正常 / 一段时间一直爆"。
//  抓栈确认,爆的时候调用链是:
//      QPersistentModelIndex::data
//        ← QAccessibleWidget::isValid
//        ← Qt 的 Windows UIA 无障碍桥
//        ← UiaReturnRawElementProvider
//  即:系统里有程序(屏幕取词、输入法、划词翻译、远程协助……)在按
//  UI Automation 协议读我们这个窗口,它一问"这表格有多少格",
//  Qt 的 UIA 桥就逐行去取 SizeHintRole / FontRole。
//
//  **这不是虚拟化失效**,而是别人越过视图直接问了模型。视口该画的
//  仍然只有 12 行 —— 所以判据要盯"单帧碰了多少行",不要盯"累计总量"。
//  累计总量只能当参考,它受外部环境影响,不能当唯一判据。
// ===========================================================================
class BigTableView : public QTableView
{
    Q_OBJECT

public:
    explicit BigTableView(QWidget *parent = nullptr);

    void setBigModel(BigModel *m);

    //  切换模型(比如换成代理模型)之后必须重新装 delegate 和列宽。
    //  setModel() 会清空所有 per-column delegate 设置。
    void installDelegatesForCurrentModel();

    // ---- 自检 ----
    quint64 paintCalls() const { return m_paintCalls; }   // paintEvent 次数
    quint64 sizeHintCalls() const { return m_sizeHintCalls; }   // 行高询问次数
    void    resetCounters();

    //  可见行范围。这个数字是虚拟化的核心证据:
    //  100 万行里,视图永远只要这几十行。
    int visibleRowCount() const;

    //  把剩余宽度补给最后一列。resizeEvent 里延后一轮调,也可以在
    //  setModel 之后手动调一次(那时还没有 resize 事件)。
    void distributeColumnWidths();

signals:
    //  画了几帧之后发一次,让外层刷新统计数字。
    //  节流逻辑在 eventFilter 里(每 8 帧),不是每帧都发。
    void statsChanged();

protected:
    void paintEvent(QPaintEvent *e) override;

    //  viewport 的 Paint 事件才代表"真的画了一帧内容"。
    //  见 BigTableView.cpp 里那段注释——QTableView::paintEvent 数不到。
    bool eventFilter(QObject *watched, QEvent *ev) override;

    //  窗口宽度变了以后,把剩余宽度补给最后一列。
    //  这样既不用 stretchLastSection(它会算进滚动条导致裁字),
    //  也不会在右侧留一块和视口不同色的空档。
    void resizeEvent(QResizeEvent *e) override;

    //  视图问"第 row 行多高"时会走这里。重载它只为了计数——
    //  这本身就是个危险的钩子:如果你在里面遍历所有行,
    //  虚拟化就废了。所以这里必须只做"查缓存/返回固定值"。
    int sizeHintForRow(int row) const override;

private:
    void installDelegates();

    //  列宽分配表最大支持多少列。固定的数组长度,比 QVector 省一次堆分配,
    //  也让 distributeColumnWidths 里的表可以写成 static const。
    static constexpr int kMaxCols = 8;

    BigModel *m_model = nullptr;

    mutable quint64 m_paintCalls    = 0;
    mutable quint64 m_sizeHintCalls = 0;
};
