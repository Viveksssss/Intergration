#pragma once

#include <QByteArray>
#include <QDrag>
#include <QMimeData>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>

#include <utility>

// 拖放协议的"数据格式"。拖出方和接收方各写一遍同样的字符串很容易打错，
// 收口成一个常量。
inline const char *const kCardMimeType = "application/x-kanban-card";

// ===========================================================================
//  自定义 QDrag：多挂两个字段
//
//  QDrag 只要活到拖放结束，Qt 就会自己管理它的生命周期。但我们在
//  dropEvent 里还需要知道：
//    · 拖的是哪张卡（cardId）
//    · 按下位置相对卡片左上角的偏移（grabOffset）—— 幽灵卡片才能跟手，
//      而不是"卡片左上角粘在鼠标上"
//
//  这些信息塞进 QMimeData 也行（下面 CardMimeData 就是干这个的），但
//  QDrag 子类的写法更直接：源控件在 dropEvent 里 qobject_cast 一次就拿到。
// ===========================================================================
class CardDrag : public QDrag
{
public:
    CardDrag(QObject *source, int cardId, const QPoint &grabOffset,
             const QSize &cardSize)
        : QDrag(source)
        , cardId(cardId)
        , grabOffset(grabOffset)
        , cardSize(cardSize)
    {
    }

    int    cardId;       // 被拖的卡片 id
    QPoint grabOffset;   // 按下位置 相对 卡片左上角
    QSize  cardSize;     // 卡片原始尺寸（拖拽幽灵图按这个尺寸画）
};

// ===========================================================================
//  一次拖放用的 payload
//
//  Qt 的标准做法是把数据写进 QMimeData：接收方只认 MIME 类型，不关心
//  数据是哪个类给的。这样即使将来把拖放接到别的进程（甚至别的程序），
//  协议都不用改。这里只放一个卡片 id —— 位置信息由接收方自己算，
//  因为"放到哪一行"取决于接收方的布局，源端根本不可能知道。
// ===========================================================================
class CardMimeData : public QMimeData
{
public:
    explicit CardMimeData(int cardId)
        : m_cardId(cardId)
    {
    }

    QStringList formats() const override
    {
        return QStringList() << QString::fromLatin1(kCardMimeType);
    }

    bool hasFormat(const QString &m) const override
    {
        return m == QString::fromLatin1(kCardMimeType);
    }

    int cardId() const { return m_cardId; }

private:
    int m_cardId;
};

// ===========================================================================
//  一次"落点"的描述：放到哪一列、第几个位置
//
//  这是整个工程里最重要的一个结构体。它出现在三个地方：
//    · 拖动过程中：告诉各列"现在该把哪个空隙让出来"
//    · 松手时：告诉数据模型"把卡片插到哪"
//    · 自检时：打印出来核对
//  三处用同一个结构体，就不会出现"看到的落点和实际的落点不一样"。
// ===========================================================================
struct DropTarget
{
    enum class Kind {
        None,     // 不在任何列上
        Place,    // 在某列的第 index 个位置（0 = 最上面）
        Append    // 放在某列末尾
    };

    Kind kind = Kind::None;
    int  col  = -1;      // 列下标
    int  index = -1;     // 插入位置（Kind::Place 时有效）

    bool isValid() const { return kind != Kind::None && col >= 0; }

    QString toString() const
    {
        switch (kind) {
        case Kind::Place:
            return QStringLiteral("col=%1 index=%2").arg(col).arg(index);
        case Kind::Append:
            return QStringLiteral("col=%1 append").arg(col);
        case Kind::None:
            break;
        }
        return QStringLiteral("none");
    }

    bool operator==(const DropTarget &o) const
    {
        return kind == o.kind && col == o.col && index == o.index;
    }
    bool operator!=(const DropTarget &o) const { return !(*this == o); }
};
