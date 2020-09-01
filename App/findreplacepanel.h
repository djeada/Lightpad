#ifndef FINDREPLACEPANEL_H
#define FINDREPLACEPANEL_H

#include <QWidget>

namespace Ui {
    class FindReplacePanel;
}


class  FindReplacePanel : public QWidget
{
    Q_OBJECT
    public:
        FindReplacePanel(bool onlyFind = true, QWidget *parent = nullptr);
        ~FindReplacePanel();
        void toggleExtensionVisibility();
        void setReplaceVisibility(bool flag);
        bool isOnlyFind();

    private slots:
        void on_more_clicked();
        void on_find_clicked();

        void on_close_clicked();

private:
        QWidget* extension;
        Ui::FindReplacePanel *ui;
        bool onlyFind;
};

#endif // FINDREPLACEPANEL_H
