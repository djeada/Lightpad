#ifndef DOCUMENTSYMBOLS_H
#define DOCUMENTSYMBOLS_H

#include <QList>
#include <QString>

namespace DocumentSymbols {

enum class Kind { Function, Method, Class, Heading };

struct Symbol {
  QString name;
  Kind kind;
  int line;
  int column;
};

QList<Symbol> scan(const QString &text, const QString &language);

} // namespace DocumentSymbols

#endif
