#include "runconfigurations.h"
#include "ui_runconfigurations.h"

RunConfigurations::RunConfigurations(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::runconfigurations)
{
    ui->setupUi(this);
    show();
}

RunConfigurations::~RunConfigurations()
{
    delete ui;
}
