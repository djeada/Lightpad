#ifndef SQLLINT_H
#define SQLLINT_H

#include "dbcatalog.h"

#include <QString>
#include <QVector>

struct SqlIssue {
  enum class Severity { Info, Warning, Error };
  Severity severity = Severity::Warning;
  int start = 0;
  int length = 0;
  QString message;
};

namespace SqlLint {

QVector<SqlIssue> lint(const QString &sql, DbEngine engine,
                       const DbSchema *schema);

}

#endif
