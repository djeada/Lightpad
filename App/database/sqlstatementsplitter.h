#ifndef SQLSTATEMENTSPLITTER_H
#define SQLSTATEMENTSPLITTER_H

#include "dbtypes.h"

#include <QString>
#include <QVector>

struct SqlStatement {
  QString text;
  int start = 0;
  int end = 0;
  int line = 0;
};

enum class SqlStatementKind {
  Empty,
  Select,
  Explain,
  Show,
  Dml,
  Ddl,
  Transaction,
  Session,
  Call,
  Other
};

namespace SqlStatementSplitter {

QVector<SqlStatement> split(const QString &script, DbEngine engine);

SqlStatement statementAt(const QString &script, int offset, DbEngine engine);

SqlStatementKind classify(const QString &statement, DbEngine engine);

bool isReadOnly(SqlStatementKind kind);

QString destructiveWarning(const QString &statement, DbEngine engine);

QString stripComments(const QString &sql);

QString connectionDirective(const QString &script);

} // namespace SqlStatementSplitter

#endif
