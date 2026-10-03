#ifndef SQLLINT_H
#define SQLLINT_H

#include "dbcatalog.h"

#include <QString>
#include <QVector>

struct SqlIssue {
  enum class Severity { Info, Warning, Error };
  Severity severity = Severity::Warning;
  int start = 0; // offset inside the linted text
  int length = 0;
  QString message;
};

// Catalog-aware static checks. Everything is heuristic and conservative: when
// the catalog is empty or not loaded, only structural checks run.
namespace SqlLint {

QVector<SqlIssue> lint(const QString &sql, DbEngine engine,
                       const DbSchema *schema);

} // namespace SqlLint

#endif
