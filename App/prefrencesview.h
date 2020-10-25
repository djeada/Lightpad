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

private:
    Ui::PrefrencesView *ui;
    MainWindow* parentWindow;
};

#endif // PREFRENCESVIEW_H
