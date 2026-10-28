#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "annotation.h"
#include <QWidget>

class QButtonGroup;
class QToolButton;
class ToolBar : public QWidget
{
    Q_OBJECT
public:
    explicit ToolBar(QWidget *parent = nullptr);

    void setCurrentTool(Tool t);
    void setUndoEnabled(bool on);

    void placeNear(const QRect&sel,const QRect&bounds);
signals:
    void toolChanged(Tool t);
    void undoRequested();
    void clearRequested();
    void copyRequested();
    void saveRequested();
    void cancelRequested();

protected:
    void paintEvent(QPaintEvent*e)  override;

private:
    QToolButton *addToolButton(Tool t, const QString &tip);
    QToolButton *addActionButton(const char *icon, const QString &tip);

    QButtonGroup *_group = nullptr;
    QToolButton  *_undo  = nullptr;
    QList<QToolButton *> _toolButtons;

};

#endif // TOOLBAR_H
