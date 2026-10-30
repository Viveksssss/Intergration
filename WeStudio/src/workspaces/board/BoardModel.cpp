#include "BoardModel.h"

#include "BoardTheme.h"

BoardModel BoardModel::demo()
{
    BoardModel m;
    m.m_cols.resize(4);

    m.m_cols[0].title  = QStringLiteral("待办");
    m.m_cols[0].accent = BoardTheme::kTagBlue;
    m.m_cols[1].title  = QStringLiteral("进行中");
    m.m_cols[1].accent = BoardTheme::kTagOrange;
    m.m_cols[2].title  = QStringLiteral("待评审");
    m.m_cols[2].accent = BoardTheme::kTagPurple;
    m.m_cols[3].title  = QStringLiteral("已完成");
    m.m_cols[3].accent = BoardTheme::kTagGreen;

    struct Def { int col; const char *title; const char *tag; const QString footer; };
    const Def defs[] = {
        { 0, "重构抓屏模块",     "重构", QStringLiteral("#101  2 天") },
        { 0, "选区边框在深色背景上看不清", "Bug",  QStringLiteral("#104  5 天") },
        { 0, "补充 README 截图", "文档", QStringLiteral("#107  1 周") },
        { 0, "拖拽幽灵图边缘发虚", "UI",   QStringLiteral("#109  1 周") },
        { 0, "支持 Esc 取消截图", "体验", QStringLiteral("#112  2 周") },

        { 1, "手工布局动画",     "核心", QStringLiteral("#203  今天") },
        { 1, "跨列拖放命中测试", "核心", QStringLiteral("#205  今天") },
        { 1, "边缘自动滚动",     "核心", QStringLiteral("#206  昨天") },

        { 2, "撤销栈改为增量方案", "重构", QStringLiteral("#301  3 天") },
        { 2, "多显示器 dpr 混用", "Bug",  QStringLiteral("#303  4 天") },

        { 3, "遮罩用奇偶填充挖洞", "已完成", QStringLiteral("#001  1 周") },
        { 3, "中点法平滑画笔",     "已完成", QStringLiteral("#002  1 周") },
        { 3, "马赛克先缩后放",     "已完成", QStringLiteral("#003  6 天") },
        { 3, "文字走 QLineEdit",   "已完成", QStringLiteral("#004  5 天") },
    };

    int id = 100;
    for (const Def &d : defs) {
        Card c;
        c.id    = id++;
        c.title = QString::fromUtf8(d.title);
        c.tag   = QString::fromUtf8(d.tag);
        c.footer = d.footer;

        if (c.tag == QStringLiteral("Bug"))
            c.tagColor = BoardTheme::kTagRed;
        else if (c.tag == QStringLiteral("核心"))
            c.tagColor = BoardTheme::kTagOrange;
        else if (c.tag == QStringLiteral("重构"))
            c.tagColor = BoardTheme::kTagBlue;
        else if (c.tag == QStringLiteral("已完成"))
            c.tagColor = BoardTheme::kTagGreen;
        else
            c.tagColor = BoardTheme::kTagPurple;

        m.m_cols[d.col].cards.append(c);
    }
    return m;
}

bool BoardModel::locate(int cardId, int *col, int *row) const
{
    for (int c = 0; c < m_cols.size(); ++c) {
        const QVector<Card> &cards = m_cols[c].cards;
        for (int r = 0; r < cards.size(); ++r) {
            if (cards[r].id == cardId) {
                if (col) *col = c;
                if (row) *row = r;
                return true;
            }
        }
    }
    return false;
}

const Card *BoardModel::card(int cardId) const
{
    int c = 0, r = 0;
    if (!locate(cardId, &c, &r))
        return nullptr;
    return &m_cols[c].cards[r];
}

bool BoardModel::moveCard(int cardId, int toCol, int toIndex)
{
    int fromCol = -1, fromRow = -1;
    if (!locate(cardId, &fromCol, &fromRow))
        return false;
    if (toCol < 0 || toCol >= m_cols.size())
        return false;

    // 注意：toIndex 是"插到目标列的第几个位置"。
    // 同列内往后移时，先把自己摘掉会让目标下标往前挪一位，所以要修正。
    if (fromCol == toCol) {
        if (toIndex == fromRow || toIndex == fromRow + 1)
            return false;                       // 原地不动
        if (toIndex > fromRow)
            --toIndex;
    }

    const Card c = m_cols[fromCol].cards.takeAt(fromRow);
    toIndex = qBound(0, toIndex, m_cols[toCol].cards.size());
    m_cols[toCol].cards.insert(toIndex, c);
    return true;
}

void BoardModel::insertCard(int col, int row, const Card &c)
{
    if (col < 0 || col >= m_cols.size())
        return;
    m_cols[col].cards.insert(qBound(0, row, m_cols[col].cards.size()), c);
}
