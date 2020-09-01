#include "findreplacepanel.h"
#include "ui_findreplacepanel.h"
#include <QVBoxLayout>
#include <QDebug>

FindReplacePanel::FindReplacePanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FindReplacePanel)
{
    ui->setupUi(this);

     show();

     ui->wholeWords->setVisible(false);
     ui->searchBackward->setVisible(false);


     ui->find->setFixedSize(ui->more->width(), ui->find->height());
}

FindReplacePanel::~FindReplacePanel() {
    delete ui;
}

void FindReplacePanel::on_more_clicked() {
    ui->wholeWords->setVisible(!ui->wholeWords->isVisible());
    ui->searchBackward->setVisible(!ui->searchBackward->isVisible());

}

void FindReplacePanel::on_find_clicked()
{

}
