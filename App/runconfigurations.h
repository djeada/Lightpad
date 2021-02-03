#ifndef RUNCOFIGURATIONS_H
#define RUNCOFIGURATIONS_H

#include <QDialog>
#include <QLineEdit>
#include <QIcon>
#include <QToolButton>

class LineEditIcon : public QLineEdit
{
    Q_OBJECT

public:
    LineEditIcon(QWidget* parent = nullptr);
    ~LineEditIcon();
    void setIcon(QIcon icon);

protected:
    virtual void paintEvent(QPaintEvent *pe) override;
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
    explicit RunConfigurations(QWidget *parent = nullptr);
    ~RunConfigurations();

private:
    Ui::runconfigurations *ui;
};

#endif // RUNCOFIGURATIONS_H
