#ifndef PREFRENCESVIEW_H
#define PREFRENCESVIEW_H

#include <QWidget>

namespace Ui {
class PrefrencesView;
}

class PrefrencesView : public QWidget
{
    Q_OBJECT

public:
    explicit PrefrencesView(QWidget *parent = nullptr);
    ~PrefrencesView();

private:
    Ui::PrefrencesView *ui;
};

#endif // PREFRENCESVIEW_H
