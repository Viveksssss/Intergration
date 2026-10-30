# WeStudio

WeStudio 是一个基于 Qt 6 Widgets 的桌面项目工作台。它把项目看板、流程画布、百万行数据表、代码编辑、截图批注、媒体时间轴和局域网协作放在同一个应用窗口中。

## 工作区

- 项目看板：自绘卡片、跨列拖放、插入位置计算和布局动画。
- 流程设计：QGraphics 画布、节点缩放平移、端口拖线、类型检查与成环检测。
- 数据视图：虚拟化百万行模型、自定义进度/状态/多行文本 Delegate。
- 代码编辑：行号栏、语法高亮、括号匹配和代码折叠。
- 截图批注：框选与八手柄缩放、五种标注、撤销重做、复制和导出。
- 媒体审阅：QMediaPlayer、视频预览、时间轴定位和框选放大。
- 协作房间：TCP 消息分帧、断线补发、聊天和带背压的文件上传。

## 构建

可以直接运行仓库中的构建脚本：

```powershell
.\build.ps1
```

也可以手动执行：

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="E:/Software/qt/qt/6.9.1/mingw_64" `
  -DCMAKE_CXX_COMPILER="E:/Software/qt/qt/Tools/mingw1310_64/bin/g++.exe" `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## 运行

```powershell
$env:PATH = "E:/Software/qt/qt/6.9.1/mingw_64/bin;$env:PATH"
.\build\WeStudio.exe
```

用 `--page` 可以直接打开某个工作区，用 `--shot` 保存一张启动截图：

```powershell
.\build\WeStudio.exe --page table --shot C:/tmp/we-studio.png
```
