#include "prefrences.h"
#include "colorpicker.h"
#include "prefrenceseditor.h"
#include "prefrencesview.h"
#include "ui_prefrences.h"
#include <QCloseEvent>

Prefrences::Prefrences(MainWindow* parent)
    : QDialog(nullptr)
    , ui(new Ui::Prefrences)
    , parentWindow(parent)
    , colorPicker(nullptr)
    , prefrencesView(nullptr)
    , prefrencesEditor(nullptr)
{

    ui->setupUi(this);

    setWindowTitle("Lightpad Prefrences");
    setupParent();
    show();
}

Prefrences::~Prefrences()
{
    delete ui;
}

void Prefrences::setTabWidthLabel(const QString& text)
{
    if (prefrencesEditor)
        prefrencesEditor->setTabWidthLabel(text);
}

void Prefrences::closeEvent(QCloseEvent* event)
{
    emit destroyed();
    event->accept();
}

void Prefrences::on_toolButton_clicked()
{
    close();
}

void Prefrences::setupParent()
{
    if (parentWindow) {
        colorPicker = new ColorPicker(parentWindow->getTheme(), parentWindow);
        prefrencesView = new PrefrencesView(parentWindow);
        prefrencesEditor = new PrefrencesEditor(parentWindow);
        ui->tabWidget->addTab(prefrencesView, "View");
        ui->tabWidget->addTab(prefrencesEditor, "Editor");
        ui->tabWidget->addTab(colorPicker, "Font " + QString(u8"\uFF06") + " Colors");
    }
}
