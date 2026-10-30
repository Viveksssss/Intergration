#pragma once

#include <QHash>
#include <QTextBlock>
#include <QVector>

class QTextDocument;

// ---------------------------------------------------------------------------
//  代码折叠。
//
//  Qt 的 QPlainTextEdit 没有内建折叠，但有一个入口：
//  QTextBlock::setVisible(false)。配合 QPlainTextDocumentLayout，
//  隐藏的 block 高度会变成 0，后面的行自动上移 —— 视觉上就是折叠了。
//
//  这里的职责只有三件：
//    1. 扫描文档，算出哪些「左括号行」到「右括号行」可以折叠（scanRegions）
//    2. 记住每个折叠区是展开还是折叠（存在 block 的 userState 上）
//    3. 把折叠状态翻译成 setVisible（applyVisibility）
// ---------------------------------------------------------------------------
class FoldManager
{
public:
    struct Region
    {
        int  startBlock = -1;   // 含 { 的那一行
        int  endBlock   = -1;   // 配对 } 的那一行
        bool collapsed  = false;
        int  hiddenLines() const { return endBlock - startBlock; }
    };

    void setDocument(QTextDocument *doc);

    // 文本变了以后重新扫描。会保留已有的折叠状态（状态存在 block 上，不在这里）。
    void rescan();

    const QVector<Region> &regions() const { return m_regions; }

    bool hasFoldAt(int blockNumber) const { return m_startIndex.contains(blockNumber); }
    bool isCollapsedAt(int blockNumber) const;

    // 折叠 / 展开（blockNumber 必须是折叠区起点）
    void toggle(int blockNumber);
    void setCollapsed(int blockNumber, bool collapsed);
    void foldAll();
    void unfoldAll();

    // 如果 blockNumber 落在某个已折叠区域的内部（不含首尾），返回那个区域，否则 -1
    int  collapsedRegionContaining(int blockNumber) const;

    // 把 m_regions 里的 collapsed 状态写回 QTextBlock::setVisible()
    void applyVisibility();

private:
    void scanRegions();

    QTextDocument   *m_doc = nullptr;
    QVector<Region>  m_regions;
    QHash<int, int>  m_startIndex;   // startBlock -> m_regions 下标
};
