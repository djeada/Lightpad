#include "prefrencesview.h"
#include "mainwindow.h"
#include "ui_prefrencesview.h"

PrefrencesView::PrefrencesView(MainWindow* parent)
    : QWidget(nullptr)
    , ui(new Ui::PrefrencesView)
    , parentWindow(parent)
{
    ui->setupUi(this);

    if (parentWindow) {
        auto settings = parentWindow->getSettings();
        ui->checkBoxBracket->setChecked(settings.matchingBracketsHighlighted);
        ui->checkBoxCurrentLine->setChecked(settings.lineHighlighted);
        ui->checkBoxLineNumbers->setChecked(settings.showLineNumberArea);
    }
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
