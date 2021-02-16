#ifndef PREFRENCESVIEW_H
#define PREFRENCESVIEW_H

#include <QWidget>

class MainWindow;

namespace Ui {
class PrefrencesView;
}

class PrefrencesView : public QWidget
{
    Q_OBJECT

public:
    PrefrencesView(MainWindow* parent);
    ~PrefrencesView();

private slots:
    void on_checkBoxLineNumbers_clicked(bool checked);
    void on_checkBoxCurrentLine_clicked(bool checked);
    void on_checkBoxBracket_clicked(bool checked);
    
private:
    Ui::PrefrencesView *ui;
    MainWindow* parentWindow;
};

#endif // PREFRENCESVIEW_H
