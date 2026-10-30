#pragma once

#include <QWidget>

#include "Annotation.h"

class QButtonGroup;
class QToolButton;

// ===========================================================================
//  浮动工具栏：一排图标按钮，跟着选区跑。
//
//  它不是一个独立窗口，而是 overlay 的**子控件**。
//  子控件的好处：坐标就是 overlay 坐标，跟着选区算位置很直接，
//  而且 grab() 截图时会一起被渲染出来（自检用得上）。
// ===========================================================================
class ToolBar : public QWidget
{
    Q_OBJECT

public:
    explicit ToolBar(QWidget *parent = nullptr);

    void setCurrentTool(Tool t);
    void setUndoEnabled(bool on);

    // 放到选区下方；下方放不下就翻到上方
    void placeNear(const QRect &sel, const QRect &bounds);

signals:
    void toolChanged(Tool t);
    void undoRequested();
    void clearRequested();
    void copyRequested();
    void saveRequested();
    void cancelRequested();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QToolButton *addToolButton(Tool t, const QString &tip);
    QToolButton *addActionButton(const char *icon, const QString &tip);

    QButtonGroup *m_group = nullptr;
    QToolButton  *m_undo  = nullptr;
    QList<QToolButton *> m_toolButtons;
};
