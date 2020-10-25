#include "prefrences.h"
#include "ui_prefrences.h"
#include "colorpicker.h"
#include "prefrencesview.h"

Prefrences::Prefrences(MainWindow *parent) :
    QDialog(nullptr),
    ui(new Ui::Prefrences),
    parentWindow(parent)
{
    ui->setupUi(this);

    setWindowTitle("Lightpad Prefrences");

    if (parentWindow) {
        ColorPicker* colorPicker = new ColorPicker(parentWindow->getTheme());
        colorPicker->setParentWindow(parentWindow);

        ui->tabWidget->addTab(new PrefrencesView(), "View");
        ui->tabWidget->addTab(new PrefrencesView(), "Editor");
        ui->tabWidget->addTab(colorPicker, "Font " + QString(u8"\uFF06") + " Colors");
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

void Prefrences::on_toolButton_clicked()
{
    close();
}
