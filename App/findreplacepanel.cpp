#include "findreplacepanel.h"
#include "ui_findreplacepanel.h"
#include "textarea.h"

#include <QVBoxLayout>
#include <QDebug>
#include <QTextDocument>

FindReplacePanel::FindReplacePanel(bool onlyFind, QWidget *parent) :
    QWidget(parent),
    document(nullptr),
    textArea(nullptr),
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

void FindReplacePanel::setOnlyFind(bool flag)
{
    onlyFind = flag;
}

void FindReplacePanel::setDocument(QTextDocument *doc)
{
    document = doc;
}

void FindReplacePanel::setTextArea(TextArea *area)
{
    textArea = area;
}

void FindReplacePanel::setFocusOnSearchBox()
{
    ui->searchFind->setFocus();
}

void FindReplacePanel::on_more_clicked()
{
    ui->options->setVisible(!ui->wholeWords->isVisible());
}

void FindReplacePanel::on_find_clicked()
{
    if (textArea)
        textArea->updateSyntaxHighlightTags(ui->searchFind->text());
}

void FindReplacePanel::on_close_clicked()
{
    textArea->updateSyntaxHighlightTags();
    close();
}
