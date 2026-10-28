#pragma once

#include <QHash>
#include <QTextBlock>
#include <QVector>

class QTextDocument;

class FoldManager
{
public:
    struct Region
    {
        int  startBlock = -1;   // 含 { 的那一行
        int  endBlock   = -1;   // 配对 } 的那一行
        bool collapsed  = false;
        int  hiddenLines() const { return endBlock - startBlock - 1; }
    };

    void setDocument(QTextDocument *doc);

    // 文本变了以后重新扫描。会保留已有的折叠状态（状态存在 block 上，不在这里）。
    void rescan();

    const QVector<Region> &regions() const { return _regions; }

    bool hasFoldAt(int blockNumber) const { return _startIndex.contains(blockNumber); }
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

    QTextDocument   *_doc = nullptr;
    QVector<Region>  _regions;
    QHash<int, int>  _startIndex;   // startBlock -> _regions 下标
    // 正在 applyVisibility 里触发重排版时置位。
    // 重排版可能同步引发一次文本变化通知，进而回到这里，直接挡掉。
    bool             _applying = false;
};
