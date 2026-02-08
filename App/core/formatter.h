#ifndef FORMATTER_H
#define FORMATTER_H

#include <QString>

class Formatter {
public:
  Formatter(QString text, const QString &languageId);

private:
  QString text;
  QString languageId;
};

#endif // FORMATTER_H
