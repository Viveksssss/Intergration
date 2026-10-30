#pragma once

#include <QMainWindow>

class BigModel;
class BigTableView;
class QLabel;
class QLineEdit;

// ===========================================================================
//  TablePage —— 标题条 + 工具条(行数/过滤) + 表格 + 状态条
// ===========================================================================
class TablePage : public QMainWindow
{
    Q_OBJECT

public:
    explicit TablePage(QWidget *parent = nullptr);

    BigTableView *view() const { return m_view; }
    BigModel     *model() const { return m_model; }

    //  自检用:直接改行数,验证"模型变了视图会不会跟"
    void setRowCount(int n);

    //  状态条上那行字。自检会读它。
    QString statusText() const;

private:
    void buildUi();
    void updateStatus();

    BigModel     *m_model = nullptr;
    BigTableView *m_view  = nullptr;
    QLineEdit    *m_filter = nullptr;
    QLabel       *m_stat   = nullptr;
};
