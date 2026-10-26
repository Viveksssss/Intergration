#include "foldmanager.h"

#include <QSet>
#include <QTextDocument>
#include <algorithm>

namespace {

inline bool isFolded(const QTextBlock&b){
    return b.userState() > 0 && (b.userState() & 1);
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
            b.setUserState(collapsed ? 1 : 0);
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
            b.setUserState(1);
    }
    applyVisibility();
}

void FoldManager::unfoldAll()
{
    for (Region &r : _regions) {
        r.collapsed = false;
        QTextBlock b = _doc->findBlockByNumber(r.startBlock);
        if (b.isValid())
            b.setUserState(0);
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
    if (!_doc)
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
    // 2. 只改「状态真的变了」的那些 block。
    for (QTextBlock b = _doc->firstBlock(); b.isValid(); b = b.next()) {
        const bool wantVisible = !hidden.contains(b.blockNumber());
        b.setVisible(wantVisible);
    }
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
    for(QTextBlock b = _doc->firstBlock();b.isValid() ;b = b.next()){
        const int bn = b.blockNumber();
        const QString text =b.text();
        const  bool folded = isFolded(b);
        for(int i =0 ;i<text.size();++i){
            const QChar ch = text.at(i);

            if (ch == '/' && i+1<text.size() && text.at(i+1) == '/'){
                break;
            }
            if (ch == '"' || ch == '\''){
                const QChar quote = ch;
                ++i;
                while(i < text.size()&&text.at(i) != quote){
                    if (text.at(i) == '\\')
                        ++i;
                    ++i;
                }
                continue;
            }
            if (ch == '{'){
                stack.append({bn,folded});
            }else if (ch == '}'){
                if (stack.empty()){
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
