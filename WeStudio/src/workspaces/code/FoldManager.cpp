#include "FoldManager.h"

#include <QSet>
#include <QTextDocument>

#include <algorithm>

namespace {

// 折叠标记存在 QTextBlock::userState() 的 bit0 上。
// 注意：userState() 的默认值是 -1（不是 0），所以不能直接 & 1，
// 必须先判断 > 0，否则每一行都会被当成「已折叠」。
inline bool isFolded(const QTextBlock &b)
{
    return b.userState() > 0 && (b.userState() & 1);
}

}  // namespace

void FoldManager::setDocument(QTextDocument *doc)
{
    m_doc = doc;
    rescan();
}

void FoldManager::rescan()
{
    scanRegions();
    applyVisibility();
}

// ---------------------------------------------------------------------------
//  扫描折叠区：一个栈扫完整个文档。
//   遇到 '{' —— 记住它在哪一行，压栈
//   遇到 '}' —— 弹出最近一个 '{'，若两者不在同一行，就形成一个可折叠区
// ---------------------------------------------------------------------------
void FoldManager::scanRegions()
{
    m_regions.clear();
    m_startIndex.clear();
    if (!m_doc)
        return;

    struct Open { int block; bool folded; };
    QVector<Open> stack;

    for (QTextBlock b = m_doc->firstBlock(); b.isValid(); b = b.next()) {
        const int     bn     = b.blockNumber();
        const QString text   = b.text();
        const bool    folded = isFolded(b);

        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);

            // 单行注释：后面的内容对括号统计没有意义，直接不看这一行了
            if (ch == '/' && i + 1 < text.size() && text.at(i + 1) == '/')
                break;

            // 字符串 / 字符字面量：里面的括号是内容不是结构，跳过
            if (ch == '"' || ch == '\'') {
                const QChar quote = ch;
                ++i;
                while (i < text.size() && text.at(i) != quote) {
                    if (text.at(i) == '\\')   // 转义字符，连下一个一起跳
                        ++i;
                    ++i;
                }
                continue;
            }

            if (ch == '{') {
                stack.append({ bn, folded });
            } else if (ch == '}') {
                if (stack.isEmpty())
                    continue;                       // 多余的右括号，忽略
                const Open top = stack.takeLast();
                if (bn <= top.block)
                    continue;                       // 同一行里的 {}，不构成折叠区

                Region r;
                r.startBlock = top.block;
                r.endBlock   = bn;
                r.collapsed  = top.folded;

                // 一行里如果开了多个 {，只认第一个 —— 否则同一行会出现多个
                // 起点相同的折叠区，界面上没法区分。
                if (!m_startIndex.contains(r.startBlock)) {
                    m_startIndex.insert(r.startBlock, m_regions.size());
                    m_regions.append(r);
                }
            }
        }
    }

    // 按起点排序，方便二分/遍历（也方便调试时看输出）
    std::sort(m_regions.begin(), m_regions.end(),
              [](const Region &a, const Region &b) { return a.startBlock < b.startBlock; });
    m_startIndex.clear();
    for (int i = 0; i < m_regions.size(); ++i)
        m_startIndex.insert(m_regions.at(i).startBlock, i);
}

bool FoldManager::isCollapsedAt(int blockNumber) const
{
    const auto it = m_startIndex.constFind(blockNumber);
    if (it == m_startIndex.constEnd())
        return false;
    return m_regions.at(it.value()).collapsed;
}

void FoldManager::setCollapsed(int blockNumber, bool collapsed)
{
    const auto it = m_startIndex.constFind(blockNumber);
    if (it == m_startIndex.constEnd())
        return;

    Region &r = m_regions[it.value()];
    r.collapsed = collapsed;

    if (m_doc) {
        QTextBlock b = m_doc->findBlockByNumber(blockNumber);
        if (b.isValid())
            b.setUserState(collapsed ? 1 : 0);   // 标记跟着 block 走，插入/删除行都不会丢
    }
    applyVisibility();
}

void FoldManager::toggle(int blockNumber)
{
    setCollapsed(blockNumber, !isCollapsedAt(blockNumber));
}

void FoldManager::foldAll()
{
    for (Region &r : m_regions) {
        r.collapsed = true;
        QTextBlock b = m_doc->findBlockByNumber(r.startBlock);
        if (b.isValid())
            b.setUserState(1);
    }
    applyVisibility();
}

void FoldManager::unfoldAll()
{
    for (Region &r : m_regions) {
        r.collapsed = false;
        QTextBlock b = m_doc->findBlockByNumber(r.startBlock);
        if (b.isValid())
            b.setUserState(0);
    }
    applyVisibility();
}

int FoldManager::collapsedRegionContaining(int blockNumber) const
{
    for (int i = 0; i < m_regions.size(); ++i) {
        const Region &r = m_regions.at(i);
        if (r.collapsed && blockNumber > r.startBlock && blockNumber <= r.endBlock)
            return i;
    }
    return -1;
}

void FoldManager::applyVisibility()
{
    if (!m_doc)
        return;

    // 1. 先算出「哪些 block 应该被藏起来」
    QSet<int> hidden;
    for (const Region &r : m_regions) {
        if (!r.collapsed)
            continue;
        for (int i = r.startBlock + 1; i <= r.endBlock; ++i)
            hidden.insert(i);
    }

    // 2. 只改「状态真的变了」的那些 block。
    //
    //    这一步很关键：如果每次都无脑 setVisible(true)，QTextDocument 会
    //    为这些 block 重建 layout，而 layout 里存的语法着色（additionalFormats）
    //    会被清空 —— 表现就是折叠一次之后，某些行的颜色突然没了，
    //    而且再也不会自己回来。只在变化时改，普通打字时一个 block 都不碰。
    for (QTextBlock b = m_doc->firstBlock(); b.isValid(); b = b.next()) {
        const bool wantVisible = !hidden.contains(b.blockNumber());
        if (b.isVisible() != wantVisible)
            b.setVisible(wantVisible);
    }
}
