#ifndef FORMATTER_H
#define FORMATTER_H

#include <QString>

#include "mainwindow.h"

class Formatter {
public:
    Formatter(QString text, Lang lang);

private:
    QString text;
};

#endif // FORMATTER_H
