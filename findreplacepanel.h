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
        FindReplacePanel(QWidget *parent = nullptr);
        ~FindReplacePanel();
        void toggleExtensionVisibility();

    private slots:
        void on_more_clicked();
        void on_find_clicked();

    private:
        QWidget* extension;
        Ui::FindReplacePanel *ui;
};

#endif // FINDREPLACEPANEL_H
