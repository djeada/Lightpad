#ifndef COLORPICKER_H
#define COLORPICKER_H

#include <QDialog>
#include "theme.h"

namespace Ui {
class ColorPicker;
}

class ColorPicker : public QDialog
{
    Q_OBJECT

public:
    explicit ColorPicker(Theme theme, QWidget *parent = nullptr);
    ~ColorPicker();

private slots:
    void on_buttonFontChooser_clicked();

private:
    Ui::ColorPicker *ui;
};

#endif // COLORPICKER_H
