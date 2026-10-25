#pragma once

#include <QMainWindow>

#include "NodeItems/NodeItem.h"
#include "NodeItems/type.h"

class NodeScene;
class NodeView;
class QLabel;

// ---------------------------------------------------------------------------
//  MainWindow —— 把场景和视图装进窗口，再挂一条工具栏。
//
//  这里没有任何绘制和交互逻辑，只做三件事：
//    ① 组装：场景 + 视图 + 工具栏 + 状态栏
//    ② 连线：把工具栏动作接到场景的方法上
//    ③ 状态：图变了就刷新"节点 N · 连线 N"
//
//  判断标准很朴素：如果某个功能改起来要动 MainWindow，说明它放错地方了。
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void buildDemoGraph();

private slots:
    void updateStatus();

private:
    void buildToolBar();
    void addNodeOfKind(Kind kind);
    QPointF viewCenter() const;

    NodeScene *m_scene = nullptr;
    NodeView  *m_view  = nullptr;
    QLabel    *m_info  = nullptr;
    int        m_spawn = 0;      // 连续添加节点时错开位置，避免叠在一起
};
