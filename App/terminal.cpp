#include "terminal.h"
#include "ui_terminal.h"

Terminal::Terminal(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::Terminal)
{
    ui->setupUi(this);
    setupTextEdit();
}

Terminal::~Terminal()
{
    delete ui;
}

void Terminal::setupTextEdit()
{
    ui->textEdit->setPlainText("elo");
    ui->textEdit->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
}
