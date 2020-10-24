#ifndef PREFRENCES_H
#define PREFRENCES_H

#include <QDialog>
#include "mainwindow.h"

namespace Ui {
class Prefrences;
}

class Prefrences : public QDialog
{
    Q_OBJECT

public:
    Prefrences(MainWindow *parent);
    ~Prefrences();

protected:
    void closeEvent( QCloseEvent* event );

private:
    Ui::Prefrences *ui;
    MainWindow* parentWindow;
};

#endif // PREFRENCES_H
