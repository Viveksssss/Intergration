#include "BigModel.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QLocale>

// ===========================================================================
//  纯函数伪随机:同样的 (row, salt) 永远得到同样的值。
//
//  为什么不用 QRandomGenerator?因为它是有状态的——每次调用都往前推进。
//  视图滚动一下、重绘一次,同一行就会变成另一个值,表格会"闪烁"。
//  这里用整数散列(类似 Wang hash),纯函数、无状态、够便宜。
// ===========================================================================
namespace {

inline quint32 mix(quint32 x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

inline quint32 rnd(int row, int salt)
{
    return mix(quint32(row) * 2654435761U + quint32(salt) * 40503U);
}

//  任务名的词库。故意做小(十几项),靠组合出 100 万种不重复的名字。
const char *const kVerbs[] = {
    "编译", "打包", "同步", "校验", "导出", "清理", "归档", "压缩",
    "分析", "重建", "迁移", "备份", "解码", "切片", "合并"
};
const char *const kNouns[] = {
    "资源清单", "纹理图集", "依赖索引", "日志分片", "模型权重", "中间产物",
    "符号表", "配置快照", "缓存块", "增量补丁", "断言记录", "测试用例"
};
const char *const kSuffix[] = {
    "批次 A", "批次 B", "批次 C", "夜间任务", "回归集", "灰度组"
};

const char *const kDescTpl[] = {
    "从远端拉取增量后重新计算摘要，与上一次快照逐块比对。",
    "遍历全部条目并校验长度字段，遇到截断就记一条告警继续走。",
    "按时间窗口切分输入，逐片处理完再合并，避免一次性占用内存。",
    "重建索引文件，顺带把失效的指针回收掉。",
    "先写临时文件再原子替换，中途失败不会留下半个结果。",
    "把这一批产物的指纹汇总成一份清单，供下一环节做差集。",
};

inline constexpr int kVerbCount   = int(sizeof(kVerbs)   / sizeof(kVerbs[0]));
inline constexpr int kNounCount   = int(sizeof(kNouns)   / sizeof(kNouns[0]));
inline constexpr int kSuffixCount = int(sizeof(kSuffix)  / sizeof(kSuffix[0]));
inline constexpr int kDescCount   = int(sizeof(kDescTpl) / sizeof(kDescTpl[0]));

}  // namespace

// ===========================================================================
//  构造
// ===========================================================================
BigModel::BigModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_rows(1000000)
{
}

int BigModel::rowCount(const QModelIndex &parent) const
{
    //  树形模型里 parent 有效表示"某个节点的子节点";
    //  表格是平的,parent 有效就该返回 0,否则视图会去要越界的行。
    if (parent.isValid())
        return 0;
    return m_rows;
}

int BigModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColCount;
}

// ===========================================================================
//  row(r):把第 r 行"算"出来
//
//  注意这里没有任何容器查找。给定 r,五次哈希 + 几次取模,几十纳秒。
//  这就是 100 万行不占内存的全部秘密。
// ===========================================================================
BigModel::RowData BigModel::row(int r) const
{
    RowData d;
    if (r < 0 || r >= m_rows)
        return d;

    const quint32 h0 = rnd(r, 1);
    const quint32 h1 = rnd(r, 2);
    const quint32 h2 = rnd(r, 3);

    d.id = r + 1;

    //  名字:动词 + 名词 + 批次,三选一组合。用 % 取模比除快一点。
    d.name = QString::fromUtf8(kVerbs[h0 % kVerbCount])
           + QString::fromUtf8(kNouns[(h0 >> 8) % kNounCount])
           + QStringLiteral(" · ")
           + QString::fromUtf8(kSuffix[(h0 >> 16) % kSuffixCount]);

    //  描述:取一句模板,有时再拼一句,凑出"可能两行甚至三行"的效果。
    //  故意让长短不齐,才能看出 delegate 的换行和省略做得对不对。
    d.desc = QString::fromUtf8(kDescTpl[h1 % kDescCount]);
    if (h1 & 1u)
        d.desc += QString::fromUtf8(kDescTpl[(h1 >> 8) % kDescCount]);

    //  状态:分布刻意不均匀——多数已完成,少量失败,才像真数据。
    //  h2 % 100:  0..59 完成  60..79 进行中  80..91 排队  92..99 失败
    const quint32 s = (h2 >> 4) % 100u;
    if (s < 60)
        d.status = Done;
    else if (s < 80)
        d.status = Running;
    else if (s < 92)
        d.status = Queued;
    else
        d.status = Failed;

    //  进度要和状态自洽:完成的必须 100,排队的一律 0。
    //  这种"字段之间有约束"的地方最容易被忽略,画出来就露馅。
    switch (d.status) {
    case Done:    d.progress = 100; break;
    case Queued:  d.progress = 0;   break;
    case Failed:  d.progress = int((h2 >> 8) % 90u); break;   // 卡在半路
    default:      d.progress = int(1 + (h2 >> 8) % 98u); break;   // 1..98
    }

    //  体积:0.4 MB ~ 4 GB 之间,用 3 位有效数字,后面按单位缩放。
    d.size = qint64(400000) + qint64(rnd(r, 4) % 4000000000u);

    return d;
}

// ===========================================================================
//  textAt():某一格显示什么文字
//
//  data() 和 delegate 都走这里。**不要在 data() 里另写一套格式化**——
//  两条路径各写一份,迟早不一致:模型算出来 "1.2 GB",delegate 自己
//  又格式化一遍变成 "1.20 GB",截图和自检永远对不上。
// ===========================================================================
QString BigModel::textAt(int r, int c) const
{
    const RowData d = row(r);
    switch (c) {
    case ColId:     return QString::number(d.id);
    case ColName:   return d.name;
    case ColStatus: return statusText(d.status);
    case ColDesc:   return d.desc;
    case ColProgress:
        //  进度列的"文字"就是百分比。delegate 会把它画在进度条上。
        return QStringLiteral("%1%").arg(d.progress);
    case ColSize: {
        //  单位换算。qint64 → 人类可读,保留一位小数。
        static const char *const unit[] = { "B", "KB", "MB", "GB", "TB" };
        double v = double(d.size);
        int u = 0;
        while (v >= 1024.0 && u < 4) {
            v /= 1024.0;
            ++u;
        }
        return QStringLiteral("%1 %2")
            .arg(v, 0, 'f', u == 0 ? 0 : 1)
            .arg(QLatin1String(unit[u]));
    }
    default:
        return QString();
    }
}

// ===========================================================================
//  data():视图来要一格数据
//
//  这个函数会被调用得非常频繁。100 万行的表如果每行都问一遍,
//  就是 600 万次调用——所以视图**只在需要时**问,这也是虚拟化的另一半。
// ===========================================================================
QVariant BigModel::data(const QModelIndex &index, int role) const
{
    //  计数放在最前面:无论哪个 role、是否有效,都算一次调用。
    //  自检靠这个数字证明"滚动一屏只问了几十次,而不是 100 万次"。
    ++m_dataCalls;

    if (!index.isValid())
        return QVariant();
    const int r = index.row();
    const int c = index.column();
    if (r < 0 || r >= m_rows || c < 0 || c >= ColCount)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        return textAt(r, c);

    case Qt::TextAlignmentRole:
        //  数字列右对齐。返回 int 而不是 Qt::Alignment 是历史包袱,
        //  Qt 内部按 int 存,直接返回 int 最省事。
        if (c == ColId || c == ColSize)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);

    case Qt::ToolTipRole:
        //  描述列被截断时,鼠标停上去能看全文。
        //  注意:tooltip 只在真正悬停时才请求,不是每行都算,不心疼。
        if (c == ColDesc)
            return textAt(r, c);
        return QVariant();

    case ProgressRole:
        return row(r).progress;

    case StatusRole:
        return row(r).status;

    case RowIdRole:
        return row(r).id;

    case SizeRole:
        return QVariant::fromValue<qint64>(row(r).size);

    default:
        return QVariant();
    }
}

QVariant BigModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (o != Qt::Horizontal)
        return QVariant();

    if (role == Qt::DisplayRole) {
        switch (section) {
        case ColId:       return QStringLiteral("序号");
        case ColName:     return QStringLiteral("任务");
        case ColProgress: return QStringLiteral("进度");
        case ColStatus:   return QStringLiteral("状态");
        case ColDesc:     return QStringLiteral("说明");
        case ColSize:     return QStringLiteral("体积");
        default:          return QVariant();
        }
    }
    if (role == Qt::TextAlignmentRole) {
        if (section == ColId || section == ColSize)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QVariant();
}

// ===========================================================================
//  setRowCount():改行数
//
//  必须走 begin/endResetModel 或 begin/endInsertRows。
//  直接改 m_rows 再调 dataChanged() 是不行的——视图的位置映射全错,
//  滚动条范围不会变,点一下就可能崩。这是新手最常踩的一个。
// ===========================================================================
void BigModel::setRowCount(int n)
{
    if (n < 0)
        n = 0;
    if (n == m_rows)
        return;

    if (n < m_rows) {
        //  变少:删掉 [n, m_rows) 这一段
        beginRemoveRows(QModelIndex(), n, m_rows - 1);
        m_rows = n;
        endRemoveRows();
    } else {
        //  变多:在末尾追加 [m_rows, n)
        beginInsertRows(QModelIndex(), m_rows, n - 1);
        m_rows = n;
        endInsertRows();
    }
}

QString BigModel::statusText(int s)
{
    switch (s) {
    case Queued:  return QStringLiteral("排队");
    case Running: return QStringLiteral("进行中");
    case Done:    return QStringLiteral("完成");
    case Failed:  return QStringLiteral("失败");
    default:      return QStringLiteral("未知");
    }
}

const char *BigModel::statusName(int s)
{
    //  给自检日志用的英文名,避免控制台编码问题
    switch (s) {
    case Queued:  return "queued";
    case Running: return "running";
    case Done:    return "done";
    case Failed:  return "failed";
    default:      return "?";
    }
}
