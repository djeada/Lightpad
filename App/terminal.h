#ifndef TERMINAL_H
#define TERMINAL_H

#include <QWidget>

namespace Ui {
class Terminal;
}

class Terminal : public QWidget
{
    Q_OBJECT

public:
    explicit Terminal(QWidget *parent = nullptr);
    ~Terminal();

private:
    void setupTextEdit();
    Ui::Terminal *ui;
};

#endif // TERMINAL_H
