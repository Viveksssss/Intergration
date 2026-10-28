#include "foldmanager.h"

#include <QSet>
#include <QTextBlockUserData>
#include <QTextDocument>
#include <algorithm>

namespace {

// ---------------------------------------------------------------------------
//  折叠状态的存放位置
//
//  ⚠️ 绝对不要用 QTextBlock::setUserState() 存折叠状态！
//
//     QSyntaxHighlighter::setCurrentBlockState() 写的就是 userState，
//     而 CodeHighlighter 用 InBlockComment = 1 标记「这一行在块注释内」。
//     如果折叠也用 userState & 1 表示「已折叠」，两者就是同一个 bit：
//
//        注释里的 `/* ... { ... */` → 高亮器把该行标成 userState = 1
//        → FoldManager 读到 userState & 1 → 误判「这行是折叠起点且已折叠」
//        → 注释下方整块代码被静默折叠隐藏
//        → 隐藏行数变化 → 滚动条 range 变化 → 视觉上位置突变
//
//     而且 setUserState(1) 还会反过来把高亮器的块注释状态抹掉，
//     导致折叠之后块注释染色失效。
//
//     QTextBlockUserData 是专门给「用户每块私货」用的独立存储，
//     和高亮器的 userState 完全隔离，更新时也不触发文档布局。
// ---------------------------------------------------------------------------
class FoldData : public QTextBlockUserData
{
public:
    bool collapsed = false;
};

inline bool isFolded(const QTextBlock &b)
{
    const auto *d = static_cast<const FoldData *>(b.userData());
    return d && d->collapsed;
}

// 注意：setUserData 不是 const 方法，所以这里按值收 QTextBlock（它本身是轻量句柄，
// 拷贝一份仍然指向同一个文档位置，改的是文档里的数据）。
inline void setFolded(QTextBlock b, bool collapsed)
{
    auto *d = static_cast<FoldData *>(b.userData());
    if (!d) {
        d = new FoldData;
        b.setUserData(d);        // block 接管所有权，随文档销毁
    }
    d->collapsed = collapsed;
}

}


void FoldManager::setDocument(QTextDocument*doc){
    _doc = doc;
    rescan();
}

void FoldManager::rescan()
{
    scanRegions();
    applyVisibility();
}

bool FoldManager::isCollapsedAt(int blockNumber) const
{
    const auto it = _startIndex.constFind(blockNumber);
    if (it == _startIndex.constEnd()){
        return false;
    }
    return _regions.at(it.value()).collapsed;
}

void FoldManager::toggle(int blockNumber)
{
    setCollapsed(blockNumber, !isCollapsedAt(blockNumber));
}

void FoldManager::setCollapsed(int blockNumber, bool collapsed)
{
    const auto it = _startIndex.constFind(blockNumber);
    if (it == _startIndex.constEnd())
        return;
    Region&r = _regions[it.value()];
    r.collapsed = collapsed;
    if (_doc){
        QTextBlock b = _doc->findBlockByNumber(blockNumber);
        if (b.isValid()){
            setFolded(b, collapsed);      // 存进 userData，不碰 userState
        }
    }
    applyVisibility();
}

void FoldManager::foldAll()
{
    for (Region &r : _regions) {
        r.collapsed = true;
        QTextBlock b = _doc->findBlockByNumber(r.startBlock);
        if (b.isValid())
            setFolded(b, true);
    }
    applyVisibility();
}

void FoldManager::unfoldAll()
{
    for (Region &r : _regions) {
        r.collapsed = false;
        QTextBlock b = _doc->findBlockByNumber(r.startBlock);
        if (b.isValid())
            setFolded(b, false);
    }
    applyVisibility();
}

int FoldManager::collapsedRegionContaining(int blockNumber) const
{
    for (int i = 0; i < _regions.size(); ++i) {
        const Region &r = _regions.at(i);
        if (r.collapsed && blockNumber > r.startBlock && blockNumber <= r.endBlock)
            return i;
    }
    return -1;
}

void FoldManager::applyVisibility()
{
    if (!_doc || _applying)
        return;

    // 1. 先算出「哪些 block 应该被藏起来」
    QSet<int>hidden;
    for(const Region&r:_regions){
        if (!r.collapsed){
            continue;
        }
        for(int i = r.startBlock+1;i<r.endBlock;++i){
            hidden.insert(i);
        }
    }

    // 2. 只改「状态真的变了」的那些 block，并记下第一个变化点。
    int firstChanged = -1;
    for (QTextBlock b = _doc->firstBlock(); b.isValid(); b = b.next()) {
        const bool wantVisible = !hidden.contains(b.blockNumber());
        if (b.isVisible() == wantVisible)
            continue;                       // 没变就不碰，省下一次无谓的排版
        if (firstChanged < 0)
            firstChanged = b.position();
        b.setVisible(wantVisible);
    }

    if (firstChanged < 0)
        return;                             // 可见性没变，什么都不用做

    // 3. ⚠️ 关键的一步，缺了它就会出现「滚动时内容重复 / 大片空白」
    //
    //    QTextBlock::setVisible() 只是给 block 打了个标记，
    //    它**不会**通知 QPlainTextDocumentLayout 重新排版。
    //
    //    而 QPlainTextDocumentLayout 是「按行堆叠」的：它维护一张
    //    blockNumber -> y 坐标的表，滚动条范围、firstVisibleBlock()、
    //    contentOffset() 全都依赖这张表。隐藏了 466 行，
    //    表里却还按 605 行算，于是：
    //
    //        · 文档逻辑高度 ≠ 实际内容高度（实测 605 → 428，正确值应为 139）
    //        · 滚动条 max 仍然是折叠前的量级，能滚到内容以外
    //        · 绘制时 block 的位置和位图滚动量对不上
    //          → 旧像素没被覆盖 → 同一个 } 被反复画出来（内容重复）
    //          → 滚过头则是一片空白
    //
    //    markContentsDirty() 会让 layout 从该位置起重走一遍
    //    documentChanged()，重新累积每个 block 的 y 坐标并更新文档尺寸。
    //    从第一个变化点开始标脏就够了，前面的 block 位置是没变的。
    _applying = true;
    _doc->markContentsDirty(firstChanged,
                            qMax(1, _doc->characterCount() - firstChanged));
    _applying = false;
}

void FoldManager::scanRegions()
{
    _regions.clear();
    _startIndex.clear();
    if (!_doc){
        return;
    }

    struct Open{
        int block;
        bool folded;
    };

    QVector<Open>stack;

    // 跨行的「当前是否在块注释里」状态。
    // 以前这里只跳过 // 和字符串，没管 /* */，于是：
    //     /*  用法示例 {  */
    //       └── 这个 { 被当成真的代码括号，和注释里的 } 配对
    //           制造出一个「不存在的折叠区」，还把后面真正的括号栈全带偏。
    bool inBlockComment = false;

    for(QTextBlock b = _doc->firstBlock();b.isValid() ;b = b.next()){
        const int bn = b.blockNumber();
        const QString text =b.text();
        const  bool folded = isFolded(b);
        for(int i =0 ;i<text.size();++i){
            const QChar ch = text.at(i);

            // ① 块注释内的一切都不参与括号配对
            if (inBlockComment){
                if (ch == QLatin1Char('*') && i+1 < text.size()
                    && text.at(i+1) == QLatin1Char('/')){
                    inBlockComment = false;
                    ++i;
                }
                continue;
            }

            // ② 注释起始：// 丢掉本行剩余，/* 进入块注释
            if (ch == QLatin1Char('/') && i+1 < text.size()){
                const QChar next = text.at(i+1);
                if (next == QLatin1Char('/')){
                    break;                       // 行注释：本行后面都忽略
                }
                if (next == QLatin1Char('*')){
                    inBlockComment = true;
                    ++i;
                    continue;
                }
            }

            // ③ 字符串 / 字符字面量整体跳过，里面的 { } 不算括号
            if (ch == QLatin1Char('"') || ch == QLatin1Char('\'')){
                const QChar quote = ch;
                ++i;
                while(i < text.size()&&text.at(i) != quote){
                    if (text.at(i) == QLatin1Char('\\'))
                        ++i;                     // 转义：跳过下一个字符
                    ++i;
                }
                continue;
            }

            if (ch == QLatin1Char('{')){
                stack.append({bn,folded});
            }else if (ch == QLatin1Char('}')){
                if (stack.isEmpty()){
                    continue;
                }
                const Open top = stack.takeLast();
                if (bn <= top.block){
                    continue;
                }
                Region r ;
                r.startBlock = top.block;
                r.endBlock = bn;
                r.collapsed = top.folded;

                if (!_startIndex.contains(r.startBlock)){
                    _startIndex.insert(r.startBlock,_regions.size());
                    _regions.append(r);
                }
            }
        }
    }
    std::sort(_regions.begin(),_regions.end(),[](const Region&a,const Region&b){
        return a.startBlock < b.startBlock;
    });
    _startIndex.clear();
    for(int i = 0;i<_regions.size();++i){
        _startIndex.insert(_regions.at(i).startBlock,i);
    }
}
