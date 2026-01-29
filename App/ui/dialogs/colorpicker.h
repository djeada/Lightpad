#ifndef COLORPICKER_H
#define COLORPICKER_H

#include "../../settings/theme.h"
#include <QDialog>
#include <QToolButton>

class MainWindow;

namespace Ui {
class ColorPicker;
}

class ColorPicker : public QDialog {
    Q_OBJECT

public:
    ColorPicker(Theme theme, MainWindow* parent);
    ~ColorPicker();
    void setParentWindow(MainWindow* window);

private slots:
    void on_buttonFontChooser_clicked();

private:
    Ui::ColorPicker* ui;
    QList<QToolButton*> colorButtons;
    MainWindow* parentWindow;
};

#endif // COLORPICKER_H
