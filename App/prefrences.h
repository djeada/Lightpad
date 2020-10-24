#ifndef PREFRENCES_H
#define PREFRENCES_H

#include <QDialog>

namespace Ui {
class Prefrences;
}

class Prefrences : public QDialog
{
    Q_OBJECT

public:
    explicit Prefrences(QWidget *parent = nullptr);
    ~Prefrences();

private:
    Ui::Prefrences *ui;
};

#endif // PREFRENCES_H
