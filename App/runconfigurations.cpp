#include "runconfigurations.h"
#include "ui_runconfigurations.h"

#include <QPainter>
#include <QStyleOption>
LineEditIcon::LineEditIcon(QWidget* parent)
    : QLineEdit(parent)
{

    QHBoxLayout* mainLayout = new QHBoxLayout();
    mainLayout->addWidget(&edit);
    mainLayout->addWidget(&button);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    setLayout(mainLayout);
    setStyleSheet("QLineEdit { border: none; background: white } QToolButton { background: white }");
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);

}

LineEditIcon::~LineEditIcon()
{
}

void LineEditIcon::setIcon(QIcon icon)
{
    button.setIcon(icon);
}

void LineEditIcon::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
  QStyleOption option;
  option.initFrom(this);
  QPainter painter(this);
  style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
};

void LineEditIcon::enterEvent(QEvent* event)
{
    setStyleSheet("QLineEdit {border-width: 1px; border-style: solid; border-color: #add8e6; }"
                  "QLineEdit#edit { border: none; background: white } QToolButton { background: white }");

    QWidget::enterEvent(event);
}

void LineEditIcon::leaveEvent(QEvent* event)
{
    setStyleSheet("QLineEdit { border: none; background: white } QToolButton { background: white }");

    QWidget::leaveEvent(event);
}

RunConfigurations::RunConfigurations(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::runconfigurations)
{
    ui->setupUi(this);
    ui->editScriptPath->setIcon(QIcon(":/resources/icons/folder.png"));
    show();
}

RunConfigurations::~RunConfigurations()
{
    delete ui;
}
