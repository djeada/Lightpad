#include "runconfigurations.h"
#include "ui_runconfigurations.h"

#include <QDir>
#include <QFileDialog>
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
    button.setCursor(Qt::ArrowCursor);
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

void LineEditIcon::connectFunctionWithIcon(void (RunConfigurations::*f)())
{
    connect(&button, &QAbstractButton::clicked, this, [&, f]() {
        auto parent = qobject_cast<RunConfigurations*>(parentWidget());
        (parent->*f)();
    });
}

void LineEditIcon::setText(const QString& text)
{
    edit.setText(text);
    edit.setCursorPosition(0);
}

QString LineEditIcon::text()
{
    return edit.text();
}

RunConfigurations::RunConfigurations(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::runconfigurations)
{
    ui->setupUi(this);
    ui->editScriptPath->setIcon(QIcon(":/resources/icons/folder.png"));
    ui->editParameters->setIcon(QIcon(":/resources/icons/add.png"));
    ui->editScriptPath->connectFunctionWithIcon(&RunConfigurations::choosePath);
    setWindowTitle("Run Configuration");
    setAttribute(Qt::WA_DeleteOnClose);
    show();
}

RunConfigurations::~RunConfigurations()
{
    delete ui;
}

void RunConfigurations::choosePath()
{
    auto scriptPath = QFileDialog::getOpenFileName(this, tr("Select script path"), QDir::homePath());
    ui->editScriptPath->setText(scriptPath);
}

QString RunConfigurations::getScriptPath()
{
    return ui->editScriptPath->text();
}

QString RunConfigurations::getParameters()
{
    return ui->editParameters->text();
}
