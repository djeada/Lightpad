#include "prefrencesview.h"
#include "ui_prefrencesview.h"
#include "mainwindow.h"

PrefrencesView::PrefrencesView(MainWindow* parent) :
    QWidget(nullptr),
    ui(new Ui::PrefrencesView),
    parentWindow(parent)
{
    ui->setupUi(this);
}

PrefrencesView::~PrefrencesView()
{
    delete ui;
}
