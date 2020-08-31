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

FindReplacePanel::~FindReplacePanel()
{
    delete ui;
}

void FindReplacePanel::on_more_clicked()
{

    if ( !ui->wholeWords->isVisible()) {
        ui->wholeWords->setVisible(true);
        ui->searchBackward->setVisible(true);
    }

    else {
        ui->wholeWords->setVisible(false);
        ui->searchBackward->setVisible(false);
    }


}

void FindReplacePanel::on_find_clicked()
{

}
