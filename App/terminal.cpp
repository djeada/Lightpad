#include "terminal.h"
#include "ui_terminal.h"

Terminal::Terminal(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Terminal)
{
    ui->setupUi(this);
    ui->textEdit->setPlainText("elo");
}

Terminal::~Terminal()
{
    delete ui;
}
