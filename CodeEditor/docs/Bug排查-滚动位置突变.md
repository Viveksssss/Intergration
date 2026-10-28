# Bug 排查记录：折叠与滚动相关的两个 Bug

本文记录两个现象不同、但都落在「折叠系统」上的 bug：

| # | 现象 | 根因 |
|---|---|---|
| 第一轮 | 轻轻一滑，视图从「54–99 行」跳到「93–129 行」 | ① 高亮器和折叠器抢 `userState` 同一个 bit<br>② 折叠解析器不认块注释，`{}` 配对错乱 |
| 第二轮 | 全部折叠后向上滚，**内容重复**（`}` 被画了五遍） | `QTextBlock::setVisible()` 不会让 `QPlainTextDocumentLayout` 重新排版 |

两轮都完整记录了排查过程、根因定位和修复，可作为 Qt 文本编辑器的典型坑参考。

---

# 第一轮：滚动时位置突然跳变

> 现象：编辑长文件时，明明是轻轻一滑，视图却从「54–99 行」直接跳到「93–129 行」。

---

## 一、先排除「不是这个原因」

遇到「滚动跳变」，第一反应通常是滚动条本身。先做实测排除：

### 实验 1：打印滚动条的真实参数

```cpp
auto *v = ed.verticalScrollBar();
printf("min=%d max=%d page=%d single=%d value=%d viewportH=%d",
       v->minimum(), v->maximum(), v->pageStep(), v->singleStep(),
       v->value(), ed.viewport()->height());
```

输出：

```
init   value=0  min=0  max=514  page=37  single=1  viewportH=688   (文件 551 行)
```

**关键结论：`QPlainTextEdit` 的垂直滚动条单位是「行」，不是像素。**
- `max = blockCount - 可见行数` = 551 - 37 = 514
- `pageStep` = 一屏能显示几行 = 37
- `singleStep` = 1 行

### 实验 2：连续滚动 60 格，检查每步增量

```cpp
QWheelEvent ev(..., QPoint(0, 0), QPoint(0, -120), ...);   // 1 notch
QApplication::sendEvent(ed.viewport(), &ev);
```

| 场景 | 结果 |
|---|---|
| NoWrap 连续 60 格 | 每格稳定 +3 行（`singleStep × wheelScrollLines`），**异常 0 次** |
| 折行模式连续 60 格 | 同样稳定，异常 0 次 |
| 折叠若干区域后连续 60 格 | 同样稳定，异常 0 次 |

**结论：滚轮滚动逻辑本身没有 bug。**

> 📌 顺带确认：用户描述的「跳 39 行」和 `pageStep = 37` 非常接近，
> 说明那次跳变跨越了将近一屏——这不是逐行滚动能产生的位移，
> 而是**内容本身的行数变了**（有行被隐藏/显示）。

---

## 二、真正的线索：显示行数对不上

用户说「54–99」→「93–129」：

| 观察 | 首行 | 末行 | 跨度 |
|---|---|---|---|
| 跳变前 | 54 | 99 | **46 行** |
| 跳变后 | 93 | 129 | **37 行** |

**同一个窗口，一屏能显示的行数不可能变。46 ≠ 37，差了 9 行。**

唯一解释：**跳变前有 9 行被折叠隐藏了**（行号栏上会跳号，但用户按「首行-末行」读数就是 46）。

所以问题不是"滚动错了"，而是**「折叠状态在滚动过程中发生了变化」** → 隐藏行数变 → 滚动条 range 变 → 视野瞬移。

---

## 三、根因：两个 bug 叠加

### 3.1 `QSyntaxHighlighter` 和 `FoldManager` 抢同一个字段

**这是本次最核心的坑。**

`QSyntaxHighlighter::setCurrentBlockState(int)` 内部写的就是 `QTextBlock::setUserState()`——

```cpp
// Qt 源码
void QSyntaxHighlighter::setCurrentBlockState(int newState)
{
    d->currentBlock.setUserState(newState);   // ← 就是它
}
```

而本项目的两处代码在抢这个字段：

```cpp
// codehighlighter.cpp —— 用 userState 存「是否在块注释内」
enum State { Normal = 0, InBlockComment = 1 };
setCurrentBlockState(InBlockComment);       // 写入 userState = 1

// foldmanager.cpp（修复前）—— 用 userState 判断「是否已折叠」
inline bool isFolded(const QTextBlock &b) {
    return b.userState() > 0 && (b.userState() & 1);   // ← 也读 bit 0
}
```

**`InBlockComment = 1` 和「已折叠 = 1」撞在同一个 bit 上。**

后果链条：

```
文件里有跨行块注释 /* ... */
   └→ 高亮器把注释内的行标成 userState = 1
      └→ FoldManager::isFolded() 读到 userState & 1 == 1
         └→ 误判「这行是折叠起点，且已经折叠了」
            └→ 该行若是 { ，对应整块代码被静默隐藏
               └→ 隐藏行数变化 → 滚动范围突变 → 视野瞬移
```

**双向污染**：`FoldManager` 的 `setUserState(1)` 还会反过来把高亮器的块注释状态抹成 0，
导致**折叠操作之后块注释染色失效**——这是同一个 bug 的另一个症状。

### 3.2 折叠解析器不识别块注释

```cpp
// 修复前的 scanRegions()：只跳过 // 和字符串，完全没管 /* */
if (ch == '/' && i + 1 < text.size() && text.at(i + 1) == '/')
    break;                                  // 行注释，跳过
if (ch == '"' || ch == '\'') { ... }        // 字符串，跳过
if (ch == '{') stack.append({bn, folded});  // ← 注释里的 { 也被当成真括号
```

于是一段很常见的注释就成了炸弹：

```cpp
/*
 * 用法示例 {
 *     foo(1, 2);
 * }
 */
```

- 注释里的 `{`（第 3 行）被压栈
- 注释里的 `}`（第 5 行）把它弹栈，**制造出一个根本不存在的折叠区**
- 更严重的是：这对假括号**消耗了真实的括号配对**，导致后续 `void foo() { ... }` 的
  `{` 找不到正确的 `}` → **所有折叠区的 startBlock / endBlock 整体错位**

错位的折叠区一旦被折叠，隐藏的可能是一两百行，而不是本该折叠的几行。

### 3.3 为什么是「有时候」

取决于两个条件同时成立：

1. 文件里有跨行块注释 `/* ... */`（C++ 项目几乎必然有）
2. 注释里恰好出现 `{` 或 `}`（写示例代码、写文档时很常见）

而且在 `foldmanager.cpp` 那侧还需要该行是 `{` 所在行，才会生成假折叠区。
所以有的文件有问题、有的没有，同一个文件里也只有特定几处 → 表现为「偶发」。

---

## 四、验证：构造最小复现用例

```cpp
#include <stdio.h>

/*
 * 用法示例 {
 *     foo(1, 2);
 * }
 */

int g;

void foo(int a, int b)
{
    if (a > 0) {
        printf("a=%d\n", a);
    }
    for (int i = 0; i < b; ++i) {
        printf("%d\n", i);
    }
}
```

写一个诊断程序，遍历所有 block 打印 `userState` 和 `foldsAreCollapsed`：

**修复前：**

```
被高亮器标记 userState != 0 的行：
  block 2    userState=1    "/*"
  block 3    userState=1    " * 用法示例 {"
  block 4    userState=1    " *     foo(1, 2);"
  block 5    userState=1    " * }"

被误判成"已折叠"的行：
  block 3    userState=1   visible=1  " * 用法示例 {"     ← 就是它！

隐藏行数 = 1 / 总行数 36
```

**修复后：**

```
被高亮器标记 userState != 0 的行：4 行（高亮功能正常保留）
被误判成"已折叠"的行：0 个
隐藏行数 = 0 / 总行数 36
```

再验证折叠范围是否与真实 `{ }` 配对一致：

| 操作 | 修复前 | 修复后 | 期望 |
|---|---|---|---|
| 折叠 block 3（注释里的 `{`） | 隐藏 1 行 | **隐藏 0 行** | 0（注释不参与） |
| 折叠 block 11（`void foo` 的 `{`） | 范围错乱 | **隐藏 6 行** | 6（12..17） |
| 折叠 block 12（`if` 的 `{`）单独 | — | **隐藏 1 行** | 1（13） |
| 折叠 block 20（行注释里的 `{`） | — | **隐藏 0 行** | 0 |

---

## 五、修复内容

### 修复 1：折叠状态改用 `QTextBlockUserData`

`foldmanager.cpp` 的匿名命名空间（**不改任何接口，`foldmanager.h` 一行没动**）：

```cpp
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

// setUserData 不是 const 方法，所以按值收 QTextBlock（轻量句柄，仍指向同一位置）
inline void setFolded(QTextBlock b, bool collapsed)
{
    auto *d = static_cast<FoldData *>(b.userData());
    if (!d) {
        d = new FoldData;
        b.setUserData(d);          // block 接管所有权
    }
    d->collapsed = collapsed;
}
```

三个调用点同步替换：

```cpp
// setCollapsed / foldAll / unfoldAll
b.setUserState(collapsed ? 1 : 0);   // ❌ 抢高亮器的字段
setFolded(b, collapsed);             // ✅ 独立存储
```

**为什么用 `QTextBlockUserData` 而不是直接存在 `QSet<int>` 里？**

因为它跟着 block 走：undo/redo、插入删除行时，Qt 会把 block 的 userData 一起搬移，
折叠状态自动跟着对应的代码走；用裸的 block 下标集合，一插入行就全错位。

### 修复 2：`scanRegions()` 增加块注释状态机

```cpp
bool inBlockComment = false;          // 跨 block 保持

for (int i = 0; i < text.size(); ++i) {
    const QChar ch = text.at(i);

    // ① 块注释内的一切都不参与括号配对
    if (inBlockComment) {
        if (ch == '*' && i + 1 < text.size() && text.at(i + 1) == '/') {
            inBlockComment = false;
            ++i;
        }
        continue;
    }

    // ② 注释起始：// 丢掉本行剩余，/* 进入块注释
    if (ch == '/' && i + 1 < text.size()) {
        const QChar next = text.at(i + 1);
        if (next == '/') break;
        if (next == '*') { inBlockComment = true; ++i; continue; }
    }

    // ③ 字符串 / 字符字面量整体跳过
    if (ch == '"' || ch == '\'') {
        const QChar quote = ch;
        ++i;
        while (i < text.size() && text.at(i) != quote) {
            if (text.at(i) == '\\') ++i;      // 转义
            ++i;
        }
        continue;
    }

    if (ch == '{') stack.append({bn, folded});
    else if (ch == '}') { ... }
}
```

> 状态必须声明在**循环外**，因为块注释可以跨几十行。写在行内循环里就只处理单行了。

### 修复 3：rescan 之后保住滚动位置

即使折叠状态是"正确的"，只要它变化了，隐藏行数就会变，滚动条 range 就会变，
`value` 会被 clamp——用户看到的就是"跳"。

```cpp
void CodeEditor::onTextChanged()
{
    ...
    // 记下折叠前的隐藏行数和顶部行
    const int hiddenBefore = foldHiddenLines();
    const QTextBlock topBlock = firstVisibleBlock();
    const int topNumber = topBlock.isValid() ? topBlock.blockNumber() : -1;

    _rescanning = true;
    _folds.rescan();
    _rescanning = false;

    // 只有折叠布局真的变了才校正，避免每次输入都白跑一遍
    if (topNumber >= 0 && foldHiddenLines() != hiddenBefore)
        scrollToBlock(topNumber);
    ...
}
```

**`scrollToBlock()` 的关键：不能直接把 blockNumber 当 value 用。**

```cpp
void CodeEditor::scrollToBlock(int blockNumber)
{
    QTextBlock target = document()->findBlockByNumber(blockNumber);
    if (!target.isValid() || !target.isVisible())
        return;              // 顶部那行自己被折进去了：别强行跳

    // 滚动条 value 的语义是「顶部行在【可见行序列】里的下标」，
    // 折叠隐藏的行不占位置，所以要数一遍它前面有多少可见行。
    int visibleIndex = 0;
    for (QTextBlock b = document()->firstBlock();
         b.isValid() && b.blockNumber() < blockNumber; b = b.next()) {
        if (b.isVisible())
            ++visibleIndex;
    }

    QScrollBar *v = verticalScrollBar();
    v->setValue(qBound(v->minimum(), visibleIndex, v->maximum()));
}
```

实测印证了这个换算关系：折叠后 `setValue(50)` 对应的其实是第 60 行
（因为前 60 行里有 10 行被折叠藏了，`60 - 10 = 50`）。

### 修复 4（顺手）：`revealCursor()` 是死代码

```cpp
// handleGutterClick() 里的注释一直是这么写的：
//   折叠后把光标移到这一行，否则光标还留在被隐藏的区域里，
//   下一帧的 revealCursor() 会立刻把它展开回来。
```

但 `revealCursor()` **从来没有被调用过**。补在光标位置变化处：

```cpp
void CodeEditor::onCursorPositionChanged()
{
    revealCursor();          // 光标落进折叠区时自动展开
    refreshExtraSelections();
    ...
}
```

修好之后，`Ctrl+G` 跳转到被折叠的行、或者折叠后点行号栏跳进隐藏区，
都能自动展开了（之前会「跳过去但看不见内容」）。

---

## 六、本次改动的文件

| 文件 | 改动 |
|---|---|
| `foldmanager.cpp` | `isFolded` / `setFolded` 改用 `QTextBlockUserData`；`setCollapsed` / `foldAll` / `unfoldAll` 同步替换；`scanRegions()` 增加块注释状态机 |
| `codeeditor.h` | 新增 `foldHiddenLines()` / `scrollToBlock()` 两个 private 方法声明 |
| `codeeditor.cpp` | `onTextChanged()` 记录并恢复滚动位置；`onCursorPositionChanged()` 补上 `revealCursor()`；新增两个辅助函数实现；`#include <QScrollBar>` |
| `foldmanager.h` | **未改动** |

> 这次必须动 `codeeditor.cpp` 的原有代码——bug 就在里面。
> 但改动都是「新增方法 + 在原函数里加几行保护」，没有重写任何既有逻辑。

---

## 七、验证结果

| 项目 | 结果 |
|---|---|
| Debug 编译 | 0 error 0 warning |
| 注释里的 `{` 是否参与折叠 | 否（隐藏 0 行） |
| `void foo { }` 折叠范围 | 正确（隐藏 6 行 = block 12..17） |
| `if { }` 单独折叠 | 正确（隐藏 1 行） |
| 行注释 `// ... {` | 不影响折叠（隐藏 0 行） |
| 高亮器 userState | 保留正常（4 行块注释仍标记为 1），不再被折叠逻辑污染 |
| 纯滚轮 60 格 | 每格稳定 3 行，异常 0 次 |

---

## 八、可以学到的 Qt 知识点

1. **`QTextBlock::userState()` 是公共资源。**
   `QSyntaxHighlighter` 的 block state 用的就是它。任何想「给每一行挂点自己的状态」的代码，
   都必须改用 `QTextBlockUserData`（`setUserData` / `userData`），否则必然和语法高亮打架。

2. **`QPlainTextEdit` 的垂直滚动条单位是「行」，不是像素。**
   `value` 的语义是「顶部行在可见行序列里的下标」。
   一旦有行被 `setVisible(false)` 隐藏，`blockNumber` 和 `value` 就不再相等，必须换算。

3. **折叠 = `QTextBlock::setVisible(false)`，而它会改变滚动范围。**
   任何"折叠状态可能变化"的操作，都要考虑 `verticalScrollBar` 的 `value` 会不会被 clamp。

4. **写代码解析器必须处理三种"不参与语法"的区域**：行注释、块注释、字符串。
   漏掉任何一个，`{}` 配对就会错乱——而且错得很隐蔽，往往表现为"折叠范围怪怪的"。

5. **「偶发 bug」要先找「数字对不上」的地方。**
   本次突破口就是「46 行 vs 37 行」——同一个窗口一屏行数不可能变，
   说明有行被藏了，直接锁定折叠系统。

---

# 第二轮：全部折叠后滚动，内容重复

> 现象（用户原话）：
> 「当全部折叠之后，整体内容向上滚动，也就是看上面的内容的时候，会出现内容重复。
> 比如原本上面是 `void func(){ }`，经过滚动之后，成了 `void func(){ } } } } }`。」

## 九、排查

### 9.1 先量化「布局是否和折叠后的内容对得上」

写一个 dump 工具，折叠前 / 折叠后分别打印三样东西：

- `document()->size().height()`（文档逻辑高度）
- 垂直滚动条的 `minimum / maximum / pageStep / value`
- `QPlainTextDocumentLayout::blockBoundingRect()`（每个 block 的 y 坐标和高度）

`foldAll()` 前后各打一次，结果非常反常：

```
折叠前      docH=605.0   vbar: val=0  max=573  page=32
foldAll 后  docH=428.0   vbar: val=0  max=395  page=33    ← 只有 605→428
```

这份文件共 605 行，`foldAll()` 之后实际被隐藏的行数是 **466**，
所以正确的逻辑高度应该是 `605 - 466 = 139`。
现在却是 428 —— **文档高度只缩了一条，和真实隐藏量对不上**。

> 这里的 `docH` 单位恰好等于「行数」，因为 `QPlainTextDocumentLayout`
> 是按行的整数倍在堆叠 block，它自己内部只维护一张 `blockNumber -> y` 的表。

### 9.2 那 428 是从哪来的

不是巧合：`setVisible(false)` 只被 layout「看见了一部分」。
`applyVisibility()` 是从上往下一行行调 `setVisible()` 的，
layout 在某些时刻会做一次**部分**重算，于是停在一个中间值上。

结论：**`QTextBlock::setVisible()` 不会通知 `QPlainTextDocumentLayout` 重新排版。**

### 9.3 为什么这个错误会表现成「内容重复」

`QPlainTextDocumentLayout` 的这张 `blockNumber -> y` 表是所有绘制的唯一依据：

| 依赖它的东西 | 出错后的表现 |
|---|---|
| 文档逻辑高度 / 滚动条 range | max 比实际大很多，能滚到内容以外 → **一片空白** |
| `firstVisibleBlock()` | 顶部算出来的是错的 block |
| `contentOffset()` | 视口原点偏移量错，绘制位置整体错位 |
| `viewport()->scroll()` 的位图滚动 | 滚动距离对，但内容位置错 → **旧像素没被覆盖 → 同一个 `}` 被反复画出来** |

而且 QPlainTextEdit 滚动时走的是「位图整体平移 + 只重绘新暴露的一条」这个优化路径，
一旦内容位置算错，被平移过去的旧像素和新画的像素就会叠在一起 ——
这就是「内容重复」的直接来源。

### 9.4 像素级复现（修复前）

折叠全部 → 滚到最底部再往回退 12 格，抓 viewport 的图：

```
┌─────────────────────────────┐
│ {    ··· 42 行              │   ← 只有 3 行有内容
│ }                           │
│                             │
│        （以下全空）          │   ← 应该是满屏代码
└─────────────────────────────┘
```

内容区在「文档尽头」之后全是空白：滚动条能滚到 395，
而实际内容只到 139。**用户看到的「重复」和「空白」是同一个根因的两种表现**，
取决于滚动位置正好落在哪里。

## 十、修复

### 修复 1（根因）：`applyVisibility()` 之后强制重排版

`QTextDocument::markContentsDirty(position, length)` 会让 layout
从该位置起重走一遍 `documentChanged()`，重新累积每个 block 的 y 坐标
并更新文档尺寸。这正是 `setVisible()` 缺的那一步。

```cpp
void FoldManager::applyVisibility()
{
    if (!_doc || _applying)
        return;

    // 1. 算出哪些 block 该被藏起来
    QSet<int> hidden;
    for (const Region &r : _regions) {
        if (!r.collapsed) continue;
        for (int i = r.startBlock + 1; i < r.endBlock; ++i)
            hidden.insert(i);
    }

    // 2. 只改「状态真的变了」的行，并记下第一个变化点
    int firstChanged = -1;
    for (QTextBlock b = _doc->firstBlock(); b.isValid(); b = b.next()) {
        const bool wantVisible = !hidden.contains(b.blockNumber());
        if (b.isVisible() == wantVisible)
            continue;
        if (firstChanged < 0)
            firstChanged = b.position();
        b.setVisible(wantVisible);
    }

    if (firstChanged < 0)
        return;                       // 可见性没变：不做任何多余工作

    // 3. 补上 layout 缺的那一步
    _applying = true;
    _doc->markContentsDirty(firstChanged,
                            qMax(1, _doc->characterCount() - firstChanged));
    _applying = false;
}
```

三个设计点：

- **从 `firstChanged` 开始标脏，不是从 0。** 前面那些 block 的位置没变，
  没必要让 layout 从头排一遍。
- **`if (firstChanged < 0) return;`** —— 每次输入 `{` `}` 都会 `rescan()`，
  但可见性通常没变。这个提前返回把「普通编辑」的开销压到零。
- **`_applying` 防重入** —— `markContentsDirty()` 可能同步引发一次
  `textChanged`，进而回到 `onTextChanged() → rescan() → applyVisibility()`。
  这一次的可见性已经是对的，直接挡掉，避免白跑一轮。

### 修复 2（体验）：折叠前后盖住「屏幕顶部那行」

布局一改，滚动条的 `max` 就变。折叠时 `max` 变小，当前 `value` 会被 clamp 到底部，
用户看到的是「我只是点了一下折叠，画面就跳到底了」。展开时反过来，画面上移。

所以在 `CodeEditor` 里加一个统一入口 `changeFold()`，
把 `handleGutterClick` / `toggleFoldAtCursor` / `foldAll` / `unfoldAll`
四个入口都收进来：

```cpp
void CodeEditor::changeFold(const std::function<void()> &change)
{
    const QTextBlock top = firstVisibleBlock();
    const int topNumber = top.isValid() ? top.blockNumber() : -1;

    change();                        // 内部 → applyVisibility → markContentsDirty

    if (topNumber >= 0)
        scrollToBlock(topNumber);    // 把顶部那一行原样拉回来

    updateGutterWidth();
    if (_gutter) _gutter->update();
    viewport()->update();
    emit foldStateChanged();
}
```

**关键点：必须记 `blockNumber`，不能记滚动条的 `value`。**
`value` 数的是「可见行」，折叠前后同一行对应的 `value` 会变，
拿 `value` 去恢复必然错位。`scrollToBlock()` 内部会重新数一遍
「目标行前面有多少可见行」，换算成新的 `value`。

## 十一、这次改动的文件

| 文件 | 改动 |
|---|---|
| `foldmanager.cpp` | `applyVisibility()` 重写：只改真正变化的行 + 记录 `firstChanged` + `markContentsDirty` 强制重排版 + `_applying` 防重入 |
| `foldmanager.h` | 新增 `bool _applying = false;` |
| `codeeditor.h` | 新增 private `changeFold(const std::function<void()> &)`；`#include <functional>` |
| `codeeditor.cpp` | 新增 `changeFold()`；`handleGutterClick` / `toggleFoldAtCursor` / `foldAll` / `unfoldAll` 改为走它（四处重复的 update + emit 收成一处） |

`onTextChanged()` 里那套「rescan 后恢复位置」的逻辑**没有改**——
它解决的是「编辑引发的重扫」，和本次「折叠引发的高度变化」是两条独立路径，
现在两条都各自有保护。

## 十二、验证结果

用 618 行的 `codeeditor.cpp` 自身做测试文件：

| 操作 | docH | max | value | 顶部 block | 隐藏行 |
|---|---|---|---|---|---|
| 初始 | 618 | 586 | 0 | 0 | 0 |
| `foldAll` | **155** | **122** | 0 | 0 | 463 |
| 滚到 60 | 155 | 122 | 60 | 235 | 463 |
| `unfoldAll` | 618 | 586 | 235 | **235** | 0 |
| 再次 `foldAll` | 155 | 122 | 60 | **235** | 463 |

- `foldAll` 后 `docH = 155 = 618 - 463` ✓（修复前是 428，错了 273 行）
- 滚到中间再 `unfoldAll`，`value` 从 60 变成 235 但**顶部 block 仍是 235** ✓，
  说明换算是按「可见行」正确做的
- 往返折叠 / 展开后隐藏行归零、文档高度复原 ✓
- 逐格 `setValue` 扫描整条滚动条，顶部 block **零次回退** ✓
- 真实滚轮上滚 12 次，每次稳定 -3 行 ✓
- **真实屏幕截图**（`QScreen::grabWindow`，不触发重绘，能看见位图残留）：
  折叠后滚到底再上滚，行号连续、无重复行、无空白 ✓

编译：0 error 0 warning。

## 十三、补充：两条需要修正的旧结论

第一轮排查（第八节）里写下的第 2、3 条知识点现在要更正：

- ❌ 旧：「折叠 = `setVisible(false)`，而它会改变滚动范围」
- ✅ 新：`setVisible(false)` 让 `blockBoundingRect()` 的高度变成 0，
  **但 QPlainTextEdit 不会因此重新排版**。滚动范围、`firstVisibleBlock()`、
  `contentOffset()` 全都还是旧的。**必须手动 `markContentsDirty()`。**

这也是「在 QPlainTextEdit 上做代码折叠」最容易被忽略的一步：
网上大量示例只写 `setVisible(false)`，在小文件、不滚动的情况下看不出问题，
一滚就露馅。

---

## 十四、这次能学到的 Qt 知识点（补）

6. **`QTextBlock::setVisible()` 和 `QPlainTextDocumentLayout` 是脱节的。**
   `setVisible` 只影响 `blockBoundingRect()` 的返回值，不会让 layout 重算位置表。
   改完必须 `QTextDocument::markContentsDirty()`。这是 QPlainTextEdit 折叠的头号坑。

7. **QPlainTextEdit 的滚动 = 位图平移 + 局部重绘。**
   所以它的正确性**完全依赖**布局表中 block 的 y 坐标。
   一旦坐标错，看到的就是「内容重复」或「一片空白」，而不是简单的错位。

8. **`QTextDocument::size().height()` 是排查布局问题的第一现场。**
   它应该始终等于「可见行数 × 行高」，对不上就说明布局没跟上内容。

9. **UI 层「保持视图位置」要记模型坐标，不要记视图坐标。**
   这里就是 `blockNumber` vs 滚动条 `value` 的区别。
   任何「筛选 / 折叠 / 排序导致可见集合变化」的场景都适用这条。
