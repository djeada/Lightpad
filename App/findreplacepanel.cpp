#include "findreplacepanel.h"
#include "ui_findreplacepanel.h"
#include <QVBoxLayout>
#include <QDebug>

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FindReplacePanel),
    onlyFind(onlyFind)
{
    ui->setupUi(this);

    show();

    ui->options->setVisible(false);
    setReplaceVisibility(onlyFind);

    ui->find->setFixedSize(ui->more->width(), ui->find->height());
}

FindReplacePanel::~FindReplacePanel() {
    delete ui;
}

void FindReplacePanel::setReplaceVisibility(bool flag)
{
    ui->widget->setVisible(flag);
    ui->replaceSingle->setVisible(flag);
    ui->replaceAll->setVisible(flag);
}

bool FindReplacePanel::isOnlyFind()
{
    return onlyFind;
}

void FindReplacePanel::on_more_clicked() {
    ui->options->setVisible(!ui->wholeWords->isVisible());

}

void FindReplacePanel::on_find_clicked()
{

}

void FindReplacePanel::on_close_clicked()
{
    close();
}
