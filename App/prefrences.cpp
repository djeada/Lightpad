#include "prefrences.h"
#include "ui_prefrences.h"
#include "colorpicker.h"
#include "prefrencesview.h"
#include "prefrenceseditor.h"
#include <QCloseEvent>

Prefrences::Prefrences(MainWindow *parent) :
    QDialog(nullptr),
    ui(new Ui::Prefrences),
    parentWindow(parent),
    colorPicker(nullptr),
    prefrencesView(nullptr),
    prefrencesEditor(nullptr) {

    ui->setupUi(this);

    setWindowTitle("Lightpad Prefrences");

    if (parentWindow) {
        colorPicker = new ColorPicker(parentWindow->getTheme(), parentWindow);
        prefrencesView =  new PrefrencesView(parent);
        prefrencesEditor = new PrefrencesEditor(parent);
        ui->tabWidget->addTab(prefrencesView, "View");
        ui->tabWidget->addTab(prefrencesEditor, "Editor");
        ui->tabWidget->addTab(colorPicker, "Font " + QString(u8"\uFF06") + " Colors");
    }

    show();
}

Prefrences::~Prefrences()
{
    delete ui;
}

void Prefrences::setTabWidthLabel(const QString &text)
{
    if (prefrencesEditor)
        prefrencesEditor->setTabWidthLabel(text);
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
