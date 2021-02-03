#ifndef RUNCOFIGURATIONS_H
#define RUNCOFIGURATIONS_H

#include <QDialog>
#include <QLineEdit>
#include <QIcon>
#include <QToolButton>

class RunConfigurations;

class LineEditIcon : public QLineEdit
{
    Q_OBJECT

public:
    LineEditIcon(QWidget* parent = nullptr);
    ~LineEditIcon();
    void setIcon(QIcon icon);
    void connectFunctionWithIcon(void (RunConfigurations::*f)());

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    QLineEdit edit;
    QToolButton button;
    virtual void enterEvent(QEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
};

namespace Ui {
class runconfigurations;
}

class RunConfigurations : public QDialog
{
    Q_OBJECT

public:
    RunConfigurations(QWidget *parent = nullptr);
    ~RunConfigurations();
    void choosePath();

private:
    Ui::runconfigurations *ui;
    QString scriptPath;
};

#endif // RUNCOFIGURATIONS_H
