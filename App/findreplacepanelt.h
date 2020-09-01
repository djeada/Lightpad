#ifndef FINDREPLACEWIDGET_H
#define FINDREPLACEWIDGET_H

#include <QWidget>

namespace Ui {
    class FindReplacePanel;
}

class FindReplacePanel : public QWidget
{
    Q_OBJECT

public:
    explicit FindReplacePanel(QWidget *parent = nullptr);
    ~FindReplacePanel();

private:
    Ui::FindReplacePanel *ui;
    QWidget* extension;
};

#endif // FINDREPLACEWIDGET_H
