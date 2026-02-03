#ifndef FORMATTER_H
#define FORMATTER_H

#include <QString>

#include "../ui/mainwindow.h"

class Formatter {
public:
  Formatter(QString text, Lang lang);

private:
  QString text;
};

#endif // FORMATTER_H
