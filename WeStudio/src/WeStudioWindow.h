#pragma once

#include <QMainWindow>
#include <QVector>

class QButtonGroup;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class WeStudioWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit WeStudioWindow(QWidget *parent = nullptr);

    bool showPage(const QString &key);

private:
    struct PageInfo
    {
        QString key;
        QString title;
        QString subtitle;
        QPushButton *button = nullptr;
        QWidget *widget = nullptr;
    };

    void buildUi();
    void addPage(const QString &key, const QString &navText,
                 const QString &title, const QString &subtitle,
                 QWidget *page);
    void setCurrentPage(int index);

    QStackedWidget *m_stack = nullptr;
    QButtonGroup *m_navGroup = nullptr;
    QVBoxLayout *m_navLayout = nullptr;
    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSubtitle = nullptr;
    QLabel *m_pageIndex = nullptr;
    QVector<PageInfo> m_pages;
};
