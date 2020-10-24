#include "prefrences.h"
#include "ui_prefrences.h"
#include "colorpicker.h"

Prefrences::Prefrences(MainWindow *parent) :
    QDialog(nullptr),
    ui(new Ui::Prefrences),
    parentWindow(parent)
{
    ui->setupUi(this);

    if (parentWindow) {
        ColorPicker* colorPicker = new ColorPicker(parentWindow->getTheme());
        colorPicker->setParentWindow(parentWindow);

        ui->tabWidget->addTab(nullptr, "view");
        ui->tabWidget->addTab(nullptr, "edit");
        ui->tabWidget->addTab(colorPicker, "colors & font");
    }

    show();
}

Prefrences::~Prefrences()
{
    delete ui;
}

void Prefrences::closeEvent( QCloseEvent* event )
{
    emit destroyed();
    event->accept();
}
