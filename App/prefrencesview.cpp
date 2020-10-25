#include "prefrencesview.h"
#include "ui_prefrencesview.h"

PrefrencesView::PrefrencesView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PrefrencesView)
{
    ui->setupUi(this);
}

PrefrencesView::~PrefrencesView()
{
    delete ui;
}
