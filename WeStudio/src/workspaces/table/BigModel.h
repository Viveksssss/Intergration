#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

// ===========================================================================
//  BigModel —— 100 万行的表格模型，但**一行业务数据都不存**。
//
//  这是这张表最重要的一件事。100 万行 × 每行一个 QString + 两个 int,
//  光对象开销就上百 MB;如果每行还存一段描述文本,直接几个 G。
//  所以这里的做法是:数据按"行号"即时算出来。
//
//      值 = f(row, column)
//
//  f 必须是纯函数——同样的入参永远得到同样的输出。这样:
//    · rowCount() 想返回多少就返回多少,内存占用恒定
//    · 视图要第 900000 行,现算一次就够,不用提前 materialize
//    · 排序/过滤也可以只改"行号映射",不搬数据(见 s6)
//
//  代价:每次 data() 都算一遍。所以 f 要便宜——用整数哈希,别用
//  qHash(QString) 这种要遍历字符串的。
// ===========================================================================
class BigModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    //  列定义。顺序就是显示顺序,加列只改这个 enum 和下面两张表。
    enum Column {
        ColId = 0,        // #  纯数字,右对齐
        ColName,          // 任务名
        ColProgress,      // 进度  0~100,用进度条画
        ColStatus,        // 状态  用彩色圆点画
        ColDesc,          // 描述  多行文本
        ColSize,          // 体积  右对齐,带单位
        ColCount          // 列数(永远放最后,当常量用)
    };

    //  一行的"完整快照"。只在需要时临时算出来,不长期持有。
    struct RowData {
        int     id      = 0;
        QString name;
        QString desc;
        int     progress = 0;   // 0..100
        int     status   = 0;   // 0=排队 1=进行中 2=完成 3=失败
        qint64  size     = 0;   // 字节
    };

    explicit BigModel(QObject *parent = nullptr);

    // ---- 自定义 role ----
    //
    //  标准 role 只有"显示文字/对齐/颜色"这几个,没办法表达"这一格是 42%"。
    //  如果 delegate 靠解析 DisplayRole 的字符串("42%" → 42)去拿数值,
    //  有两处代价:
    //    · 一次 QString → int 解析,每格一次
    //    · 更糟的是"状态"要从隔壁列问 —— index.sibling() 会多一次
    //      完整的 data() 调用。100 万行 × 每格一次,白白多一倍开销。
    //
    //  自定义 role 让模型一次就把"结构化的值"给出去,delegate 不用解析、
    //  不用跨格。这就是 QAbstractItemModel 留给上层扩展的正当接口。
    enum Role {
        ProgressRole = Qt::UserRole + 1,   // int  0..100
        StatusRole,                        // int  BigModel::Status
        RowIdRole,                         // int  1-based 序号
        SizeRole                           // qint64 字节数
    };

    // ---- QAbstractTableModel 必须实现的四个 ----
    int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation o,
                        int role = Qt::DisplayRole) const override;

    // ---- 行数可以改:用来演示"模型变了视图怎么跟" ----
    void setRowCount(int n);
    int  totalRows() const { return m_rows; }

    // ---- 取整行 / 取单格 ----
    //  这两个是 delegate 和自检的入口。delegate 拿着 index 想知道
    //  "这一行是不是失败状态",不必再走一遍 data() 的 role 分发。
    RowData row(int r) const;
    //  按列取文本。DisplayRole 内部也走这个,保证两条路径永远一致。
    QString textAt(int r, int c) const;

    // ---- 自检用 ----
    //  为了证明"真的是按需算、没有缓存",这里统计 data() 被调了几次。
    quint64 dataCalls() const { return m_dataCalls; }
    void    resetDataCalls() const { m_dataCalls = 0; }

    //  状态枚举给 delegate 和自检共用,避免两边各写一份魔数。
    enum Status { Queued = 0, Running = 1, Done = 2, Failed = 3 };
    static QString statusText(int s);
    static const char *statusName(int s);

private:
    int m_rows = 0;

    //  mutable:重置计数是 const 操作(data() 是 const 的)
    mutable quint64 m_dataCalls = 0;
};
