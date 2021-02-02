#include "terminal.h"
#include "ui_terminal.h"
#include <QStyle>

Terminal::Terminal(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::Terminal)
{
    ui->setupUi(this);
    ui->closeButton->setIcon(qApp->style()->standardIcon(QStyle::SP_TitleBarCloseButton));
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

void Terminal::on_closeButton_clicked()
{
    emit destroyed();
    close();
}
