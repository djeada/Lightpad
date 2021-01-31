#ifndef RUNCOFIGURATIONS_H
#define RUNCOFIGURATIONS_H

#include <QDialog>

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
