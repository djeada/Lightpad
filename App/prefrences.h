#ifndef PREFRENCES_H
#define PREFRENCES_H

#include <QDialog>
#include "mainwindow.h"

class ColorPicker;
class PrefrencesView;
class PrefrencesEditor;

namespace Ui {
class Prefrences;
}

class Prefrences : public QDialog
{
    Q_OBJECT

public:
    Prefrences(MainWindow *parent);
    ~Prefrences();
    void setTabWidthLabel(const QString& text);

protected:
    void closeEvent( QCloseEvent* event );

private slots:
    void on_toolButton_clicked();

private:
    Ui::Prefrences *ui;
    MainWindow* parentWindow;
    ColorPicker* colorPicker;
    PrefrencesView* prefrencesView;
    PrefrencesEditor* prefrencesEditor;

};

#endif // PREFRENCES_H
