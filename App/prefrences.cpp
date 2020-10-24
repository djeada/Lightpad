#include "prefrences.h"
#include "ui_prefrences.h"

Prefrences::Prefrences(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Prefrences)
{
    ui->setupUi(this);
}

Prefrences::~Prefrences()
{
    delete ui;
}
