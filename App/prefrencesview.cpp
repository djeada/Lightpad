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

void PrefrencesView::on_checkBoxLineNumbers_clicked(bool checked)
{
    if (parentWindow)
        parentWindow->showLineNumbers(checked);
}

void PrefrencesView::on_checkBoxCurrentLine_clicked(bool checked)
{
    if (parentWindow)
        parentWindow->highlihtCurrentLine(checked);
}

void PrefrencesView::on_checkBoxBracket_clicked(bool checked)
{
    if (parentWindow)
        parentWindow->highlihtMatchingBracket(checked);
}
